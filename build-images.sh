#!/bin/bash

# Usage: ./build-images.sh [-f] (flasher only)

# Build all factory images, skip on any argument
if [ "$1" == "" ]; then
    docker build -t esp-miner-factory -f Dockerfile-factory .
    
    # TODO: Is clean necessary?
    # docker run --rm -v $PWD:/project -w /project esp-miner-factory idf.py clean
    docker run --rm -v $PWD:/project -w /project esp-miner-factory idf.py build

    for board in 102 201 202 203 204 205 401 402 403 601; do
        if [ -f "config-$board.cvs" ]; then
            echo "Building config.bin for board $board"
            docker run --rm -v $PWD:/project -w /project esp-miner-factory /opt/esp/idf/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate config-${board}.cvs config.bin 0x6000
            echo "Creating image for board $board"
            docker run --rm -v $PWD:/project -w /project esp-miner-factory /project/merge_bin.sh -c esp-miner-factory-${board}-dev.bin
        fi
    done
fi

# tell web flasher about our dev images
echo '{
    "devices": [
        {
            "name": "Max",
            "boards": [
                {
                    "name": "102",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-102-dev.bin"
                        }
                    ]
                }
            ]
        },
        {
            "name": "Ultra",
            "boards": [
                {
                    "name": "201",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-201-dev.bin"
                        }
                    ]
                },
                {
                    "name": "202",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-202-dev.bin"
                        }
                    ]
                },
                {
                    "name": "203",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-203-dev.bin"
                        }
                    ]
                },
                {
                    "name": "204",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-204-dev.bin"
                        }
                    ]
                },
                {
                    "name": "205",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-205-dev.bin"
                        }
                    ]
                }
            ]
        },
        {
            "name": "Supra",
            "boards": [
                {
                    "name": "401",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-401-dev.bin"
                        }
                    ]
                },
                {
                    "name": "402",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-402-dev.bin"
                        }
                    ]
                },
                {
                    "name": "403",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-403-dev.bin"
                        }
                    ]
                }
            ]
        },
        {
            "name": "Gamma",
            "boards": [
                {
                    "name": "601",
                    "supported_firmware": [
                        {
                            "version": "dev",
                            "path": "firmware/esp-miner-factory-601-dev.bin"
                        }
                    ]
                }
            ]
        }
    ]
}
' > build/firmware_data.json

# Build webflasher image
docker build -t bitaxe-web-flasher -f Dockerfile-webflasher .

# Open web browser to the local webflasher
if command -v xdg-open >/dev/null 2>&1; then
    # Linux
    xdg-open "http://localhost:3000"
elif command -v open >/dev/null 2>&1; then
    # macOS
    open "http://localhost:3000"
elif command -v start >/dev/null 2>&1; then
    # Windows
    start "http://localhost:3000"
fi

# Use the dev firmware in our working directory
docker run --rm --name bwf -v $PWD:/app/bitaxe-web-flasher/out/firmware -p 3000:3000 bitaxe-web-flasher

