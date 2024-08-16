#!/bin/bash

rpath="$( dirname "$( readlink -f "$0" )" )"
cd $rpath

docker run --rm -it -v /dev:/dev --privileged -v "$rpath/..":/project esp-idf-builder /bin/bash -c "bitaxetool $@"
