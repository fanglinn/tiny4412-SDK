#!/bin/bash
echo "make exynos_defconfig"
#make exynos_defconfig
echo "start ... loadaddr = 0x40008000"
make uImage LOADADDR=0x40008000 -j4
make dtbs

echo "setup"
sudo cp arch/arm/boot/uImage ~/bin/tiny4412/
sudo cp arch/arm/boot/dts/exynos4412-tiny4412.dtb ~/bin/tiny4412/
echo "finish"

