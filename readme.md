# Bitaxe test firmware

This firmware brings up all of the main components on the bitaxe v2.2.
- DS4432U+ current DAC for setting the TPS40305 output voltage
- EMC2101 fan controller for reading the ASIC die temp and controlling the fans
- INA260 power monitor for reading the bitaxe power consumption.
- OLED

### Build and Flash

Enter `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.
