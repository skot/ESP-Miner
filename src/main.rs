#![no_std]
#![no_main]
#![feature(type_alias_impl_trait)]

use embassy_executor::{Executor, Spawner, _export::StaticCell};
use embassy_net::{
    tcp::{TcpReader, TcpSocket, TcpWriter},
    Config, Ipv4Address, Stack, StackResources,
};
use embassy_sync::{
    blocking_mutex::raw::{CriticalSectionRawMutex, NoopRawMutex},
    pubsub::{PubSubChannel, Publisher, Subscriber},
    signal::Signal,
};
use embassy_time::{Duration, Timer};
use embedded_svc::wifi::{ClientConfiguration, Configuration, Wifi};
use emc2101::{EMC2101Async, Level};
#[cfg(feature = "generate-clki")]
use esp32s3_hal::ledc::{
    channel::{self, ChannelIFace},
    timer::{self, TimerIFace},
    LSGlobalClkSource, LowSpeed, LEDC,
};
use esp32s3_hal::{
    clock::{ClockControl, CpuClock},
    embassy,
    gpio::IO,
    i2c::I2C,
    interrupt,
    peripherals::{Interrupt, Peripherals, I2C0, UART1},
    prelude::*,
    timer::TimerGroup,
    uart::TxRxPins,
    Delay, Priority, Rng, Rtc, Uart,
};
use esp_backtrace as _;
use esp_println::{logger::init_logger, println};
use esp_wifi::{
    initialize,
    wifi::{WifiController, WifiDevice, WifiEvent, WifiMode, WifiState},
};
use fugit::HertzU32;

const SSID: &str = env!("SSID");
const PASSWORD: &str = env!("PASSWORD");

macro_rules! singleton {
    ($val:expr) => {{
        type T = impl Sized;
        static STATIC_CELL: StaticCell<T> = StaticCell::new();
        let (x,) = STATIC_CELL.init(($val,));
        x
    }};
}

static EXECUTOR: StaticCell<Executor> = StaticCell::new();

#[entry]
fn main() -> ! {
    init_logger(log::LevelFilter::Info);
    println!("Init!");
    let peripherals = Peripherals::take();
    let mut system = peripherals.SYSTEM.split();
    let clocks = ClockControl::configure(system.clock_control, CpuClock::Clock240MHz).freeze();

    // Disable the RTC and TIMG watchdog timers
    let mut rtc = Rtc::new(peripherals.RTC_CNTL);
    let timer_group0 = TimerGroup::new(
        peripherals.TIMG0,
        &clocks,
        &mut system.peripheral_clock_control,
    );
    let mut wdt0 = timer_group0.wdt;
    let timer_group1 = TimerGroup::new(
        peripherals.TIMG1,
        &clocks,
        &mut system.peripheral_clock_control,
    );
    let mut wdt1 = timer_group1.wdt;

    // Disable watchdog timers
    rtc.swd.disable();
    rtc.rwdt.disable();
    wdt0.disable();
    wdt1.disable();

    let wifi_timer = timer_group1.timer0;
    initialize(
        wifi_timer,
        Rng::new(peripherals.RNG),
        system.radio_clock_control,
        &clocks,
    )
    .unwrap();

    let (wifi, _) = peripherals.RADIO.split();
    let (wifi_interface, controller) = esp_wifi::wifi::new_with_mode(wifi, WifiMode::Sta);

    embassy::init(&clocks, timer_group0.timer0);

    let io = IO::new(peripherals.GPIO, peripherals.IO_MUX);
    let mut bm_rst = io.pins.gpio1.into_push_pull_output();
    bm_rst.set_low().unwrap();
    println!("Reset BM1397 RST with gpio1 LOW");

    #[cfg(not(feature = "generate-clki"))]
    let _bm_clki_freq: HertzU32 = 25u32.MHz(); // will be use to init the bm1397 chain

    #[cfg(feature = "generate-clki")]
    {
        println!("Generating BM1397 CLKI 20MHz on gpio41");
        let bm_clki_freq: HertzU32 = 20u32.MHz();
        let bm_clki = io.pins.gpio41.into_push_pull_output();
        let mut ledc = LEDC::new(
            peripherals.LEDC,
            &clocks,
            &mut system.peripheral_clock_control,
        );
        ledc.set_global_slow_clock(LSGlobalClkSource::APBClk);

        let mut lstimer0 = ledc.get_timer::<LowSpeed>(timer::Number::Timer0);

        lstimer0
            .configure(timer::config::Config {
                duty: timer::config::Duty::Duty1Bit,
                clock_source: timer::LSClockSource::APBClk,
                frequency: bm_clki_freq,
            })
            .unwrap();

        let mut channel0 = ledc.get_channel(channel::Number::Channel0, bm_clki);
        channel0
            .configure(channel::config::Config {
                timer: &lstimer0,
                duty_pct: 50,
                pin_config: channel::config::PinConfig::PushPull,
            })
            .unwrap();
    }

    // Timer::after(Duration::from_millis(100)).await;
    let mut delay = Delay::new(&clocks);
    delay.delay_ms(100u32);
    bm_rst.set_high().unwrap();
    println!("Release BM1397 RST with gpio1 HIGH");

    // will be use to init the bm1397 chain
    let bm_serial = Uart::new_with_config(
        peripherals.UART1,
        None,
        Some(TxRxPins::new_tx_rx(
            io.pins.gpio17.into_push_pull_output(),
            io.pins.gpio18.into_floating_input(),
        )),
        &clocks,
        &mut system.peripheral_clock_control,
    );
    interrupt::enable(Interrupt::UART1, Priority::Priority1).unwrap();

    // will be used to control emc2101/ina260/ds44232u
    let i2c = I2C::new(
        peripherals.I2C0,
        io.pins.gpio46,
        io.pins.gpio45,
        400u32.kHz(),
        &mut system.peripheral_clock_control,
        &clocks,
    );
    esp32s3_hal::interrupt::enable(Interrupt::I2C_EXT0, Priority::Priority1).unwrap();

    let wifi_config = Config::Dhcp(Default::default());
    let wifi_seed = 1234; // very random, very secure seed

    // Init network stack
    let stack = &*singleton!(Stack::new(
        wifi_interface,
        wifi_config,
        singleton!(StackResources::<3>::new()),
        wifi_seed
    ));

    let executor = EXECUTOR.init(Executor::new());
    executor.run(|spawner| {
        spawner.spawn(wifi_task(controller)).ok();
        spawner.spawn(stack_task(&stack)).ok();
        spawner.spawn(network_task(&stack)).ok();
        spawner.spawn(emc2101_task(i2c)).ok();
        spawner.spawn(bm1397_task(bm_serial)).ok();
    });
}

#[embassy_executor::task]
async fn wifi_task(mut controller: WifiController<'static>) {
    println!("start connection task");
    println!("Device capabilities: {:?}", controller.get_capabilities());
    loop {
        match esp_wifi::wifi::get_wifi_state() {
            WifiState::StaConnected => {
                // wait until we're no longer connected
                controller.wait_for_event(WifiEvent::StaDisconnected).await;
                Timer::after(Duration::from_millis(5000)).await
            }
            _ => {}
        }
        if !matches!(controller.is_started(), Ok(true)) {
            let client_config = Configuration::Client(ClientConfiguration {
                ssid: SSID.into(),
                password: PASSWORD.into(),
                ..Default::default()
            });
            controller.set_configuration(&client_config).unwrap();
            println!("Starting wifi");
            controller.start().await.unwrap();
            println!("Wifi started!");
        }
        println!("About to connect...");

        match controller.connect().await {
            Ok(_) => println!("Wifi connected!"),
            Err(e) => {
                println!("Failed to connect to wifi: {e:?}");
                Timer::after(Duration::from_millis(5000)).await
            }
        }
    }
}

#[embassy_executor::task]
async fn stack_task(stack: &'static Stack<WifiDevice<'static>>) {
    stack.run().await
}
#[derive(Clone)]
struct Sv1Resp {
    id: u64,
    data: &'static [u8],
}

enum TcpState {
    ErrorMaxRetry,
}

static TCP_SIGNAL: Signal<CriticalSectionRawMutex, TcpState> = Signal::new();

#[embassy_executor::task]
async fn network_task(stack: &'static Stack<WifiDevice<'static>>) {
    loop {
        if stack.is_link_up() {
            break;
        }
        Timer::after(Duration::from_millis(500)).await;
    }

    println!("Waiting to get IP address...");
    loop {
        if let Some(config) = stack.config() {
            println!("Got IP: {}", config.address);
            break;
        }
        Timer::after(Duration::from_millis(500)).await;
    }

    loop {
        Timer::after(Duration::from_millis(1_000)).await;

        let mut rx_buffer = [0; 4096];
        let mut tx_buffer = [0; 4096];
        let mut socket = TcpSocket::new(&stack, &mut rx_buffer, &mut tx_buffer);

        socket.set_timeout(Some(embassy_net::SmolDuration::from_secs(10)));

        let remote_endpoint = (Ipv4Address::new(68, 235, 52, 36), 21496);
        println!("Connecting to public-pool.io...");
        let r = socket.connect(remote_endpoint).await;
        if let Err(e) = r {
            println!("connect error: {:?}", e);
            continue;
        }
        println!("connected!");

        let (socket_rx, socket_tx) = socket.split();
        let resp_channel = PubSubChannel::<NoopRawMutex, Sv1Resp, 4, 1, 1>::new();
        let resp_sub = resp_channel.subscriber().unwrap();
        let resp_pub = resp_channel.publisher().unwrap();
        let spawner = Spawner::for_current_executor().await;
        spawner.spawn(stratum_rx_task(socket_rx, resp_pub)).ok();
        spawner.spawn(stratum_tx_task(socket_tx, resp_sub)).ok();

        TCP_SIGNAL.wait().await;

        Timer::after(Duration::from_millis(3000)).await;
        // Not sure if getting out of this block will actually kill the spawned tasks?!?
    }

    #[embassy_executor::task]
    async fn stratum_tx_task(
        socket_tx: TcpWriter<'_>,
        subscriber: Subscriber<'_, NoopRawMutex, Sv1Resp, 4, 1, 1>,
    ) {
        let mut buf = [0; 1024];
        let mut id = 0u64;
        enum Sv1ClientSetupStates {
            Configure,
            Connect,
            Authorize,
        }
        let mut state = Sv1ClientSetupStates::Configure;
        let mut retry = 0u8;
        // Setup loop
        loop {
            let s = match state {
                Sv1ClientSetupStates::Configure => stratum_v1::configure_request(id, &mut buf),
                Sv1ClientSetupStates::Connect => {
                    stratum_v1::connect_request(id, "bitaxe-rs", &mut buf)
                }
                Sv1ClientSetupStates::Authorize => stratum_v1::authorize_request(
                    id,
                    "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa.bitaxe",
                    "x",
                    &mut buf,
                ),
            };
            if let Err(e) = s {
                println!("create request error: {:?}", e);
                continue;
            }
            let s = s.unwrap();
            use embedded_io::asynch::Write;
            let r = socket_tx.write_all(&buf[..s]).await;
            if let Err(e) = r {
                println!("write request error: {:?}", e);
                retry = retry + 1;
                if retry > 5 {
                    TCP_SIGNAL.signal(TcpState::ErrorMaxRetry);
                }
                continue;
            }
            loop {
                let resp = subscriber.next_message_pure().await;
                if resp.id == id {
                    continue;
                }
                let resp = match state {
                    Sv1ClientSetupStates::Configure => {
                        stratum_v1::parse_configure_response(&resp.data)
                    }
                    Sv1ClientSetupStates::Connect => stratum_v1::parse_connect_response(&resp.data),
                    Sv1ClientSetupStates::Authorize => {
                        stratum_v1::parse_authorize_response(&resp.data)
                    }
                };
                if let Err(e) = resp {
                    println!("parse response error: {:?}", e);
                    break;
                }
                let resp = resp.unwrap();
                if let Err(e) = resp.result {
                    println!("response error: {:?}", e);
                    state = match e.code {
                        24 => Sv1ClientSetupStates::Authorize, // Unauthorized worker
                        25 => Sv1ClientSetupStates::Connect,   // Not subscribed
                        _ => state, // Other/Unknown, Job not found (=stale), Duplicate share, // Low difficulty share
                    };
                }
                break;
            }
            id = id + 1;
            // everything is fine, change state
            match state {
                Sv1ClientSetupStates::Configure => state = Sv1ClientSetupStates::Connect,
                Sv1ClientSetupStates::Connect => state = Sv1ClientSetupStates::Authorize,
                Sv1ClientSetupStates::Authorize => break,
            };
        }
        // Mine loop
        loop {
            todo!("implement mine sv1 TX loop");
        }
    }

    #[embassy_executor::task]
    async fn stratum_rx_task(
        socket_rx: TcpReader<'_>,
        publisher: Publisher<'_, NoopRawMutex, Sv1Resp, 4, 1, 1>,
    ) {
        let mut buf = [0; 1024];
        loop {
            let n = match socket_rx.read(&mut buf).await {
                Ok(0) => {
                    println!("read TCP EOF");
                    continue;
                }
                Ok(n) => n,
                Err(e) => {
                    println!("read TCP error: {:?}", e);
                    continue;
                }
            };
            // println!("{}", core::str::from_utf8(&buf[..n]).unwrap());
            let resp = stratum_v1::parse_id_response(&buf[..n]);
            if let Err(e) = resp {
                println!("parse stratum v1 error: {:?}", e);
                continue;
            }
            match resp.unwrap() {
                Some(id) => publisher.publish_immediate(Sv1Resp {
                    id: resp.id,
                    data: &buf[..n],
                }),
                None => todo!("send &buf[..n]"),
            }
        }
    }
}

#[embassy_executor::task]
async fn emc2101_task(i2c: I2C<'static, I2C0>) {
    let mut emc2101 = EMC2101Async::new(i2c).await.unwrap();
    /* Noctua PWM fan have a PWM target frequency of 25kHz, acceptable range 21kHz to 28kHz */
    /* The signal is not inverted, 100% PWM duty cycle (= 5V DC) results in maximum fan speed. */
    /* See https://noctua.at/pub/media/wysiwyg/Noctua_PWM_specifications_white_paper.pdf */
    emc2101.set_fan_pwm(25u32.kHz(), false).await.unwrap();
    #[cfg(feature = "emc2101-tach")]
    emc2101.enable_tach_input().await.unwrap();
    #[cfg(feature = "emc2101-alert")]
    emc2101.enable_alert_output().await.unwrap();
    let lvl1 = Level {
        temp: 50,
        percent: 20,
    };
    let lvl2 = Level {
        temp: 70,
        percent: 50,
    };
    let lvl3 = Level {
        temp: 80,
        percent: 70,
    };
    let lvl4 = Level {
        temp: 90,
        percent: 85,
    };
    let lvl5 = Level {
        temp: 100,
        percent: 100,
    };
    emc2101
        .set_fan_lut(&[lvl1, lvl2, lvl3, lvl4, lvl5], 3)
        .await
        .unwrap();

    loop {
        let bm_temp = emc2101.get_temp_external().await.unwrap();
        println!("BM1397 temperature: {}°C", bm_temp);
        #[cfg(feature = "emc2101-tach")]
        {
            let fan_rpm = emc2101.get_fan_rpm().await.unwrap();
            println!("FAN speed: {} rpm", fan_rpm);
        }

        Timer::after(Duration::from_millis(1000)).await;
    }
}

#[embassy_executor::task]
async fn bm1397_task(mut uart: Uart<'static, UART1>) {
    loop {
        embedded_hal_async::serial::Write::write(&mut uart, b"Hello async write!!!\r\n")
            .await
            .unwrap();
        embedded_hal_async::serial::Write::flush(&mut uart)
            .await
            .unwrap();
        Timer::after(Duration::from_millis(1_000)).await;
    }
}
