[![](https://dcbadge.vercel.app/api/server/3E8ca2dkcC)](https://discord.gg/3E8ca2dkcC)

![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/skot/esp-miner/total)
![GitHub commit activity](https://img.shields.io/github/commit-activity/t/skot/esp-miner)
![GitHub contributors](https://img.shields.io/github/contributors/skot/esp-miner)


# ESP-Miner
esp-miner is open source ESP32 firmware for the [Bitaxe](https://github.com/skot/bitaxe)

You should compile this firmware yourself. 
 
 1. Clone this repo locally
 2. Follow steps 1 - 4 from here: https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/get-started/linux-macos-setup.html
 3. If you don't know your chip's flash size look up the specs or try getting info on it using esptool (see below).
 4. From inside this project's directory  run
 ```
 idf.py menuconfig
 ```
 5. Choose `Serial flasher config` and select the correct flash size (return to menu)
 6. If your flash is 16MB you can skip step 7 as the default partition table settings will work
 7. Choose  `Partition Table` and replace the default with the correct filename depending on your flash size:
 - partitions_4mb.csv
 - partitions_16mb.csv
 8. Save and Exit
 9. If you've attempted to build this firmware before run 
 ```
 idf.py fullclean
 ```
 10. Build the firmware
 ```
 idf.py build
 ```
 11. Flash the firmware
 ```
 idf.py flash
 ```
 12. Connect to your board to see how you did:
 ```
 idf.py monitor
 ```

 # ESP Device Help
 Depending on your familiarity with esp devices you might have trouble connecting with it and/or be unaware of the specifications you need to know.  Here is how to connect and get some data.

 1. Plug in your device
 2. Find the port of your device run `ls /dev/tty*` and look for something like `/dev/ttyUSB0` or `/dev/ttyACM0` (if you're not sure - unplug > run list > plug back in > run list > find diff)
 3. See if you can detect the board with esptool by running `esptool.py chip_id`
 4. If you get an error about permisions try adding yourself to the dialout group `sudo usermod -aG dialout $USER` and/or changing the permissions of the drive temporarily `sudo chmod 666 /dev/ttyACM0` (replace ttyAMCO with the correct port on your system)
 5. Try #3 again

# Bitaxetool
We also have a command line python tool for flashing Bitaxe and updating the config called Bitaxetool 

**Bitaxetool Requires Python3.4 or later and pip**

Install bitaxetool from pip. pip is included with Python 3.4 but if you need to install it check <https://pip.pypa.io/en/stable/installation/>

```
pip install --upgrade bitaxetool
```
The bitaxetool includes all necessary library for flashing the binary file to the Bitaxe Hardware.

You need to provide a config.cvs file (see repo for examples) and the appropiate firmware.bin file in it's executed directory.

- Flash with the bitaxetool

```
bitaxetool --config ./config.cvs --firmware ./esp-miner-factory-v2.0.3.bin
```

## AxeOS API
The esp-miner UI is called AxeOS and provides an API to expose actions and information.

For more details take a look at `main/http_server/http_server.c`.

Things that can be done are:
  
  - Get System Info
  - Get Swarm Info
  - Update Swarm
  - Swarm Options
  - System Restart Action
  - Update System Settings Action
  - System Options
  - Update OTA Firmware
  - Update OTA WWW
  - WebSocket

Some API examples in curl:
  ```bash
  # Get system information
  curl http://YOUR-BITAXE-IP/api/system/info
  ```
  ```bash
  # Get swarm information
  curl http://YOUR-BITAXE-IP/api/swarm/info
  ```
  ```bash
  # System restart action
  curl -X POST http://YOUR-BITAXE-IP/api/system/restart
  ```

## Administration

The firmware hosts a small web server on port 80 for administrative purposes. Once the Bitaxe device is connected to the local network, the admin web front end may be accessed via a web browser connected to the same network at `http://<IP>`, replacing `IP` with the LAN IP address of the Bitaxe device, or `http://bitaxe`, provided your network supports mDNS configuration.
