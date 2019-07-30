TARGET	:= keys-dt

obj-m	+= $(TARGET).o

BIN_PATH = /home/flinn/bin/tiny4412/
ROOTFS = /home/flinn/tmp/rootfs
KERNEL = /home/flinn/tiny4412-SDK/linux-4.19.27
DT     = $(KERNEL)/arch/arm/boot/dts/exynos4412-tiny4412

all:
	make -C $(KERNEL) M=`pwd` modules

clean:
	make -C $(KERNEL) M=`pwd` clean
	
install:
	#make -C $(KERNEL) M=`pwd` modules_install INSTALL_MOD_PATH=$(ROOTFS)
	sudo cp $(TARGET).ko $(ROOTFS)
	sudo cp $(DT).dtb $(BIN_PATH)

dtbs:
	#dtc -I dtb -O dts tiny4412.dts -o tiny4412.dtb
	dtc -O dtb -o $(DT).dtb $(DT).dts
