export IDF_PATH=/opt/esp/idf
/opt/esp/python_env/idf5.3_py3.10_env/bin/python /opt/esp/idf/tools/idf_monitor.py -p /dev/ttyACM0 -b 115200 --toolchain-prefix xtensa-esp32s3-elf- --target esp32s3 /workspaces/build/esp-miner.elf
root@31cc35ad06b6:/workspaces# export IDF_PATH=/opt/esp/idf
root@31cc35ad06b6:/workspaces# /opt/esp/python_env/idf5.3_py3.10_env/bin/python /opt/esp/idf/tools/idf_monitor.py -p /dev/ttyACM0 -b 115200 --toolchain-prefix xtensa-esp32s3-elf- --target esp32s3 /workspaces/build/esp-miner.elf
--- esp-idf-monitor 1.4.0 on /dev/ttyACM0 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
I (115) esp_image: segment 1: paddr=00023ec4 vaddr=3fc93cESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x15 (USB_UART_CHIP_RESET),boot:0x28 (SPI_FAST_FLASH_BOOT)
Saved PC:0x40378db6
0x40378db6: esp_random at /opt/esp/idf/components/esp_hw_support/hw_random.c:84 (discriminator 1)

SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2810,len:0x178c
load:0x403c8700,len:0x4
load:0x403c8704,len:0xcb8
load:0x403cb700,len:0x2d9c
entry 0x403c8914
I (26) boot: ESP-IDF v5.3-dev-3225-g5a40bb8746 2nd stage bootloader
I (27) boot: compile time Apr 18 2024 07:48:00
I (27) boot: Multicore bootloader
I (31) boot: chip revision: v0.2
I (35) boot.esp32s3: Boot SPI Speed : 80MHz
I (40) boot.esp32s3: SPI Mode       : DIO
I (45) boot.esp32s3: SPI Flash Size : 2MB
I (49) boot: Enabling RNG early entropy source...
I (55) boot: Partition Table:
I (58) boot: ## Label            Usage          Type ST Offset   Length
I (66) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (73) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (81) boot:  2 factory          factory app      00 00 00010000 00100000
I (88) boot: End of partition table
I (92) esp_image: segment 0: paddr=00010020 vaddr=3c040020 size=13e9ch ( 81564) map
I (115) esp_image: segment 1: paddr=00023ec4 vaddr=3fc93c00 size=03228h ( 12840) load
I (119) esp_image: segment 2: paddr=000270f4 vaddr=40374000 size=08f24h ( 36644) load
I (129) esp_image: segment 3: paddr=00030020 vaddr=42000020 size=32d80h (208256) map
I (167) esp_image: segment 4: paddr=00062da8 vaddr=4037cf24 size=06c94h ( 27796) load
I (180) boot: Loaded app from partition at offset 0x10000
I (180) boot: Disabling RNG early entropy source...
I (183) cpu_start: Multicore app
I (192) cpu_start: Pro cpu start user code
I (192) cpu_start: cpu freq: 160000000 Hz
I (192) app_init: Application information:
I (195) app_init: Project name:     unit_test_stratum
I (201) app_init: App version:      07e7d41
I (206) app_init: Compile time:     Apr 21 2024 12:19:07
I (212) app_init: ELF file SHA256:  0c91dab32...
I (217) app_init: ESP-IDF:          v5.3-dev-3225-g5a40bb8746
I (223) efuse_init: Min chip rev:     v0.0
I (228) efuse_init: Max chip rev:     v0.99 
I (233) efuse_init: Chip rev:         v0.2
I (238) heap_init: Initializing. RAM available for dynamic allocation:
I (245) heap_init: At 3FC98C00 len 00050B10 (322 KiB): RAM
I (251) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (257) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (263) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM
I (271) spi_flash: detected chip: gd
I (274) spi_flash: flash io: dio
I (310) sleep: Configure to isolate all GPIO pins in sleep state
I (317) sleep: Enable automatic switching of GPIO sleep configuration
I (324) main_task: Started on CPU0
I (334) main_task: Calling app_main()

#### Running all the registered tests #####

Running Testing single BM1366 chip against a known valid block...
I (384) test_bm1366: ASIC: BM1366
I (384) test_bm1366: I2C initialized successfully
I (384) DS4432U.c: Set ASIC voltage = 1.250V [0xB9]
I (384) DS4432U.c: Writing 0xB9
I (394) serial: Initializing serial
I (394) bm1366Module: Initializing BM1366
I (1594) bm1366Module: 1 chip(s) detected on the chain
final refdiv: 2, fbdiv: 220, postdiv1: 5, postdiv2: 1, min diff value: 0.000000
I (1624) bm1366Module: Setting Frequency to 550.00MHz (0.01)
I (1624) bm1366Module: Setting max baud of 1000000 
I (1634) serial: Changing UART baud to 1000000
I (1634) test_bm1366: BM1366 is ready and waiting for work
I (1644) test_bm1366: Preparing job
I (1644) bm1366Module: Setting job ASIC mask to 511
I (1654) test_bm1366: Changing chip address and sending job; new chip address: 0xc0
I (1664) test_bm1366: Waiting for result ... (might take a while due to 60s timeout)
I (2114) test_bm1366: Result[1]: Nonce 3818226259 Nonce difficulty 8452.86377870396427169907838106155396. rolled-version 0x24148000
I (3974) test_bm1366: Result[2]: Nonce 504135883 Nonce difficulty 816.88959549147682537295622751116753. rolled-version 0x349cc000
I (5704) test_bm1366: Result[3]: Nonce 3818226259 Nonce difficulty 8452.86377870396427169907838106155396. rolled-version 0x24148000
I (7554) test_bm1366: Result[4]: Nonce 504135883 Nonce difficulty 816.88959549147682537295622751116753. rolled-version 0x349cc000
I (9294) test_bm1366: Result[5]: Nonce 3818226259 Nonce difficulty 8452.86377870396427169907838106155396. rolled-version 0x24148000
I (11144) test_bm1366: Result[6]: Nonce 504135883 Nonce difficulty 816.88959549147682537295622751116753. rolled-version 0x349cc000
I (12874) test_bm1366: Result[7]: Nonce 3818226259 Nonce difficulty 8452.86377870396427169907838106155396. rolled-version 0x24148000
I (14724) test_bm1366: Result[8]: Nonce 504135883 Nonce difficulty 816.88959549147682537295622751116753. rolled-version 0x349cc000
I (16464) test_bm1366: Result[9]: Nonce 3818226259 Nonce difficulty 8452.86377870396427169907838106155396. rolled-version 0x24148000
I (18314) test_bm1366: Result[10]: Nonce 504135883 Nonce difficulty 816.88959549147682537295622751116753. rolled-version 0x349cc000
I (20044) test_bm1366: Changing chip address and sending job; new chip address: 0xc2
I (20044) test_bm1366: Waiting for result ... (might take a while due to 60s timeout)
I (21064) test_bm1366: Result[1]: Nonce 3118696143 Nonce difficulty 1007.71313585765790321602253243327141. rolled-version 0x2915a000
I (21234) test_bm1366: Result[2]: Nonce 3529540887 Nonce difficulty 125538251293054.07812500000000000000000000000000. rolled-version 0x2a966000
I (21234) test_bm1366: Expected nonce and version match. Solution found!
/workspaces/components/bm1397/test/test_bm1366.c:23:Testing single BM1366 chip against a known valid block:PASS
Running Check coinbase tx construction...
/workspaces/components/stratum/test/test_mining.c:7:Check coinbase tx construction:PASS
Running Validate merkle root calculation...
/workspaces/components/stratum/test/test_mining.c:19:Validate merkle root calculation:PASS
Running Validate another merkle root calculation...
/workspaces/components/stratum/test/test_mining.c:43:Validate another merkle root calculation:PASS
Running Validate bm job construction...
/workspaces/components/stratum/test/test_mining.c:75:Validate bm job construction:FAIL: Element 0 Expected 77 Was 214. Function [mining]
Running Validate version mask incrementing...
/workspaces/components/stratum/test/test_mining.c:78:Validate version mask incrementing:PASS
Running Test extranonce 2 generation...
/workspaces/components/stratum/test/test_mining.c:118:Test extranonce 2 generation:PASS
Running Test nonce diff checking...
/workspaces/components/stratum/test/test_mining.c:153:Test nonce diff checking:FAIL: Expected 18 Was 0. Function [mining test_nonce]
Running Test nonce diff checking 2...
/workspaces/components/stratum/test/test_mining.c:189:Test nonce diff checking 2:FAIL: Expected 683 Was 0. Function [mining test_nonce]
Running Parse stratum method...
/workspaces/components/stratum/test/test_stratum_json.c:4:Parse stratum method:PASS
Running Parse stratum mining.notify abandon work...
/workspaces/components/stratum/test/test_stratum_json.c:21:Parse stratum mining.notify abandon work:PASS
Running Parse stratum set_difficulty params...
/workspaces/components/stratum/test/test_stratum_json.c:62:Parse stratum set_difficulty params:PASS
Running Parse stratum notify params...
/workspaces/components/stratum/test/test_stratum_json.c:71:Parse stratum notify params:PASS
Running Parse stratum mining.set_version_mask params...
/workspaces/components/stratum/test/test_stratum_json.c:108:Parse stratum mining.set_version_mask params:PASS
Running Parse stratum result success...
/workspaces/components/stratum/test/test_stratum_json.c:118:Parse stratum result success:PASS
Running Parse stratum result error...
/workspaces/components/stratum/test/test_stratum_json.c:128:Parse stratum result error:PASS
Running Test double sha...
/workspaces/components/stratum/test/test_utils.c:5:Test double sha:PASS
Running Test hex2bin...
/workspaces/components/stratum/test/test_utils.c:12:Test hex2bin:PASS
Running Test bin2hex...
/workspaces/components/stratum/test/test_utils.c:25:Test bin2hex:PASS
Running Test hex2char...
/workspaces/components/stratum/test/test_utils.c:33:Test hex2char:PASS

-----------------------
20 Tests 3 Failures 0 Ignored 
FAIL

#### Starting interactive test menu #####



Press ENTER to see the list of tests.
