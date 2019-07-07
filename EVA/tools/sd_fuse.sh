#!/bin/bash

target=../tiny4412


./mkbl2 $target bl2.bin 14336

dd iflag=dsync oflag=dsync if=E4412_N.bl1.bin of=$1 seek=1
dd iflag=dsync oflag=dsync if=./bl2.bin of=$1 seek=17
sync
