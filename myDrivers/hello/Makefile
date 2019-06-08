KERN_DIR = /home/flinn/tiny4412-SDK/tiny4412/linux-4.19.27

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= hello.o
