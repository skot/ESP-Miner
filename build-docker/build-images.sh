#!/bin/bash

PROJECT_PATH=$PWD

flash_only=0
board=""
all_boards=""

function show_help() {
    echo "Usage: build-images.sh [OPTION]"
    echo "Options:"
    echo "  -f: Only launch the web flasher"
    echo "  -b board: Build only board image. example: 601"
    echo "  -a: Build all board images"
}

if [ $# -eq 0 ]; then
    show_help
    exit 0
fi

while getopts "hfb:a" opt; do
    case "$opt" in
        f)
            flash_only=1
            ;;
        b)  board=$OPTARG
            ;;
        a)  all_boards=1
            ;;
     esac
done

if [ $flash_only -eq 0 ]; then
    docker build -t esp-miner-factory -f build-docker/Dockerfile-factory .
    
    # TODO: Is clean necessary?
    # docker run --rm -v $PWD:/project -w /project esp-miner-factory idf.py clean
    docker run --rm -v $PROJECT_PATH:/project -w /project esp-miner-factory idf.py build

    if [ $all_boards == 1 ]; then
        boards="102 201 202 203 204 205 401 402 403 601"
    else
        boards=$board
    fi

    for board in $boards; do
        if [ -f "config-$board.cvs" ]; then
            echo "Building config.bin for board $board"
            docker run --rm -v $PROJECT_PATH:/project -w /project esp-miner-factory \
                /opt/esp/idf/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
                generate config-${board}.cvs config.bin 0x6000

            echo "Creating image for board $board"
            docker run --rm -v $PROJECT_PATH:/project -w /project esp-miner-factory \
                /project/merge_bin.sh -c build-docker/esp-miner-factory-${board}-dev.bin
        fi
    done
fi

# Build webflasher image
docker build -t bitaxe-web-flasher -f build-docker/Dockerfile-webflasher .

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

# Use start the web flasher with the dev firmware in the build-docker directory
docker run --rm --name bwf \
    -v $PROJECT_PATH/build-docker:/app/bitaxe-web-flasher/out/firmware \
    -p 3000:3000 bitaxe-web-flasher

