BZM is part of the ASIC component, with portable C11 modules for the Bonanza
ASIC protocol and four-chip chain. The portable modules build on a host without
ESP-IDF or application state. They provide register
transactions, frame parsing, telemetry decoding, topology, balanced engine
activation, startup and clock sequencing, power sequencing, and work encoding.
The protocol modules use caller-owned state without heap allocation. The higher-level
mining workflow in `runtime.h` adds job tracking, scheduling, result validation,
and a singleton four-chip driver using platform allocation callbacks and POSIX
mutexes. All of these sources are independent of ESP-IDF and application state.
Within `components/asic`, BZM implementation modules live in `bzm/`, public
headers in `include/bzm/`, private headers in `private_include/bzm/`, and tests
in `test/bzm/`. Platform backends live in `bzm/port/`. Firmware builds register
all BZM sources through the ASIC component's `CMakeLists.txt`.

From the repository root, build and run the native tests with:

```sh
cmake -S components/asic/test/bzm/host -B build/bzm-host
cmake --build build/bzm-host
ctest --test-dir build/bzm-host --output-on-failure
```

The native test build compiles the portable ASIC sources and the common
`mining_job/asic_job.c` source into test support. It needs a C11 compiler, CMake,
POSIX threads, and the standard C/math libraries. It does not install or export
a separate BZM library. ESP-IDF tests in `components/asic/test/bzm` run with the
existing ASIC test suite through `bash tools/run_qemu_tests.sh`.

The public headers under `components/asic/include/bzm` separate the fixture interfaces:

- `transport.h`: bind a serial device, monotonic clock, RX clearing, and optional
  parser-state locks through `bzm_serial_ops_t`. Initialize a zeroed
  `bzm_serial_transport_t` with those callbacks and their context. Each device
  has independent parser, telemetry, reply, and result queues. A read returns
  bytes, zero on timeout, or a negative I/O error; a write must accept the whole
  buffer. `read` must honor its millisecond timeout, including a nonblocking
  read when it is zero. `now_us` must advance monotonically in microseconds.
  After chain discovery, set `asic_ids`, `asic_count`, and `engine_count` before
  calling the multicast work or flush helpers.
  `bzm_serial_poll` returns status, bytes received, and frames emitted. Timeouts
  and partial frames return `BZM_OK`; zero bytes indicate an idle poll. A zero
  timeout performs no read. Receive failures return `BZM_ERR_IO`, preserving
  counts from bytes parsed before the error, and invoke the optional
  `on_io_error` callback once. This also applies to polls inside register,
  NOOP, and result transactions. The callback must not reenter the transport;
  the runtime uses it to latch a driver fault until the next startup.
- `bringup.h` and `balanced_ramp.h`: use the same startup and balanced activation
  logic as the firmware. Supply fixture register, timing, telemetry, activation,
  and transmit-barrier callbacks. The fixture owns reset, power, and fault
  shutdown. A failed startup returns a report and never marks the chain running.
- `power.h`: drive fixture rails through `bzm_power_ops_t`. Status zero means
  success; nonzero callback errors are returned unchanged. Invalid arguments
  return `BZM_ERR_INVALID_ARG`. Failed startup or voltage validation attempts
  all shutdown steps.
- `protocol.h` and `work_registers.h`: supply a `bzm_work_t` with four prepared
  midstates, versions, nonce bounds, and header tail values. Midstates contain
  SHA-256 h0 through h7 as little-endian words. `merkle_residue` is the final
  four merkle-root bytes interpreted as a little-endian integer; timestamp and
  target are ordinary integer values. Register encoders specify byte order
  explicitly, independently of host endianness. The optional `handle` is caller
  metadata and is never sent to the ASIC.

Serial writes are the existing bridge protocol: each 9-bit word occupies two
bytes, with the low byte first and the address bit in bit zero of the second
byte. RX is the framed TDM byte stream. A host fixture must provide that bridge
protocol; a direct 8-bit UART connection is not interchangeable. This library
does not open a serial port or select GPIO pins.

The first manufacturing target is Linux with a USB/UART bridge. Bind the
callbacks to the bridge's serial device in raw mode, using `poll` for read
timeouts, `CLOCK_MONOTONIC` for time, and a complete-write loop. The bridge must
convert the byte-paired TX words into the ASIC's 9-bit UART protocol. Fixture
power and reset remain separate callbacks.

BZM has no version-mask register or hardware version rolling. Software computes
four midstates from the versions permitted by a job's mask and writes all four
to engine register `0x10`, followed by their sequence tags at `0x38` and the
start command at `0x39`. A zero mask uses the same version in all four entries;
a one-bit mask repeats its two possible versions. Enhanced mode still requires
all four entries. Startup already writes and checks engine configuration
`0x01 = 0x04`, enabling the four TCEs in enhanced mode; this does not change with
the version mask. See Intel Blockscale 1000 Series ASIC Datasheet v1.3,
sections 8.1.9.1 and 8.1.10.1, and tables 8-13 and 8-15.

One owner must serialize complete command transactions, polling, and transport
lifetime. The optional `lock` and `unlock` callbacks must be supplied together;
they protect parser/queue access for concurrent snapshot readers. Callbacks must
not reenter the transport while holding its lock. A transport cannot be copied
after initialization because its parser retains its address. Deinitialize before
rebinding callbacks. The library never closes or frees the supplied context.

`components/asic/test/bzm/host/test_transport.c` is a complete host adapter example with two independent
devices, fragmented/interleaved input, register timeouts, I/O failures, and golden
wire bytes. `test_manufacturing.c` in the same directory drives a complete four-ASIC startup and
failure scenarios through the public fixture interfaces. Every portable public header
is also compiled independently by the native test build.

`runtime.h` provides the complete portable BZM mining driver. Bind
`bzm_platform_ops_t` from `platform.h` for monotonic time, delays, serial I/O,
optional logging, paired allocation/free callbacks, and optional SHA-256
acceleration. Standard allocation and software SHA-256 are the defaults.
The `large` allocation hint lets ESP require PSRAM while Linux uses ordinary
memory. Supply board controls through `bzm_control_ops_t` from `controls.h`.
Missing control or serial callbacks fail closed. Bind once before driver use;
callbacks and contexts must remain valid for the lifetime of the driver.

For Linux, `bzm_platform_posix_defaults` fills clock, delay, and logging callbacks.
Add bridge serial and board-control callbacks before binding. The runtime owns
one chain; keep its device context alive, serialize startup and configuration,
and schedule `bzm_driver_service` about every 100 ms from the host. The library
does not create tasks. The result reader and submitter can run on separate
threads, with the driver serializing transport and job state. `bzm_device_t`
provides callbacks for submission permission, faults, and frequency updates.
Results contain owned job snapshots; copy the returned result before the next
result-read call. There is no physical Linux serial backend included yet.

The ESP implementation lives in `port/esp`: `platform.c`
binds ESP clock, allocation, SHA-256, UART and board controls; `driver.c` adapts
`GlobalState`, schedules the FreeRTOS service task, and converts portable results
to `task_result`. GPIO pin and UART buffer policies remain in that component.
The shared ASIC API and Bitmain drivers retain their ESP implementations.

The ESP RX ring remains 32 KiB. Mining polls after each logical-engine assignment,
not after a sweep of all 944 physical engines. The native `test_driver_backlog`
models the actual encoder and checkpoint with 2 Mbaud 8N1 TX and continuous RX.
One assignment writes 622 bytes and takes 3.360 ms including its 250 us gap;
with the driver's 10/100 ms pacing and no other readers, the largest modeled
poll gap is 103.360 ms and the full-wire-rate backlog is 20,672 bytes. An added
200 ms scheduling stall raises that backlog to 60,672 bytes. Parsing is
instantaneous in this model; ESP scheduling, ISR, PSRAM, and allocation delays
are not measured. These figures replace the obsolete full-dispatch estimate,
but do not establish a hardware worst case or justify a smaller ring. Measure
on-device backlog and latency before reducing it.

The common `ASIC_set_version_mask` hook configures hardware rolling and is a
no-op for BZM. The runtime consumes `asic_job_t.version_mask` when it builds
each work item and retains the four versions with the assignment, so delayed
results use their original job's versions after subsequent mask changes.
