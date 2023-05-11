# ESP-Miner -- serial_test

The ESP-Miner is a ESP32 firmware in development to mine bitcoin with [the bitaxe](https://github.com/skot/bitaxe) based on the BM1397 ASIC

## Overview

This branch, `serial_test` is just to show that the level-shifted serial communication between the ESP32 and the BM1397 is working. It's super hacky and quick, so don't judge!

### Build and Flash

Enter `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

