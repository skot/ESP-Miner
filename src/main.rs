#![no_std]
#![no_main]
#![feature(type_alias_impl_trait)]

use embassy_executor::Executor;
use embassy_time::{Duration, Timer};
#[cfg(feature = "generate-clki")]
use esp32s3_hal::ledc::{
    channel::{self, ChannelIFace},
    timer::{self, TimerIFace},
    LSGlobalClkSource, LowSpeed, LEDC,
};
use esp32s3_hal::{
    clock::ClockControl, embassy, gpio::IO, peripherals::Peripherals, prelude::*,
    timer::TimerGroup, Delay, Rtc,
};
use esp_backtrace as _;
use esp_println::println;
use static_cell::StaticCell;

#[embassy_executor::task]
async fn run1() {
    loop {
        println!("Hello world from embassy using esp-hal-async!");
        Timer::after(Duration::from_millis(1_000)).await;
    }
}

#[embassy_executor::task]
async fn run2() {
    loop {
        println!("Bing!");
        Timer::after(Duration::from_millis(5_000)).await;
    }
}

static EXECUTOR: StaticCell<Executor> = StaticCell::new();

#[entry]
fn main() -> ! {
    println!("Init!");
    let peripherals = Peripherals::take();
    let mut system = peripherals.SYSTEM.split();
    let clocks = ClockControl::boot_defaults(system.clock_control).freeze();

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

    // #[cfg(feature = "embassy-time-systick")]
    embassy::init(
        &clocks,
        esp32s3_hal::systimer::SystemTimer::new(peripherals.SYSTIMER),
    );

    #[cfg(feature = "embassy-time-timg0")]
    embassy::init(&clocks, timer_group0.timer0);

    let io = IO::new(peripherals.GPIO, peripherals.IO_MUX);
    let mut bm_rst = io.pins.gpio1.into_push_pull_output();
    bm_rst.set_low().unwrap();
    println!("Reset BM1397 RST with gpio1 LOW");
    #[cfg(feature = "generate-clki")]
    {
        println!("Generating BM1397 CLKI 20MHz on gpio41");
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
                frequency: 20u32.MHz(),
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

    let mut delay = Delay::new(&clocks);
    delay.delay_ms(100u32);
    bm_rst.set_high().unwrap();
    println!("Release BM1397 RST with gpio1 HIGH");

    let executor = EXECUTOR.init(Executor::new());
    executor.run(|spawner| {
        spawner.spawn(run1()).ok();
        spawner.spawn(run2()).ok();
    });
}
