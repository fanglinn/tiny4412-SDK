TARGET	:= leds_dt

obj-m	+= $(TARGET).o

ROOTFS = /home/flinn/tmp/rootfs
KERNEL = /home/flinn/tiny4412-SDK/linux-4.19.27

all:
	make -C $(KERNEL) M=`pwd` modules

clean:
	make -C $(KERNEL) M=`pwd` clean
	
install:
	#make -C $(KERNEL) M=`pwd` modules_install INSTALL_MOD_PATH=$(ROOTFS)
	sudo cp $(TARGET).ko $(ROOTFS)


