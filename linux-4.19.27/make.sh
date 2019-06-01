#!/bin/bash
echo "start ... loadaddr = 0x40008000"
make uImage LOADADDR=0x40008000 -j4
make dtbs
echo "finish"
