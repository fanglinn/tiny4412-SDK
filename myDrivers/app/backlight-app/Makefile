TARGET        = backlight_test

CC            = /home/flinn/tools/4.3.2/bin/arm-none-linux-gnueabi-gcc
CFLAGS        = -Wall -std=gnu99 -pthread



OBJS          = $(TARGET).o
INSTALL_PATH  = /home/flinn/tmp/rootfs

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $<

install:
	chmod 755 $(TARGET)
	cp $(TARGET) $(INSTALL_PATH) 

clean:
	rm -rf *.o $(TARGET) 
