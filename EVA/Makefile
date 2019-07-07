TARGET 	= tiny4412
BIN	= $(TARGET).bin

CROSS_COMPILE	:= /home/flinn/tools/4.5.1/bin/arm-none-linux-gnueabi-
CC		:= $(CROSS_COMPILE)gcc
LD		:= $(CROSS_COMPILE)ld
AS		:= $(CROSS_COMPILE)as
OBJCOPY		:= $(CROSS_COMPILE)objcopy -O binary
OBJDUMP		:= $(CROSS_COMPILE)objdump -D
NM		:= $(CROSS_COMPILE)nm


CFLAGS          := -Wall -O2 -nostdlib -fno-builtin \
		-I./include \
		-I./HAL \
		-I./common \
		-I./src 

OBJS := src/head.o src/main.o src/system_clock.o HAL/uart.o src/command.o


$(TARGET): $(OBJS)
	${LD} -Tbasic.lds -o $(TARGET)_elf $^
	${OBJCOPY} -O binary -S $(TARGET)_elf $@
	${OBJDUMP} -D -m arm $(TARGET)_elf > $(TARGET).dis
	$(NM) $(TARGET)_elf > system.map

%.o:%.c
	${CC}  $(CFLAGS) -c -o $@ $<

%.o:%.S
	${CC}  $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(TARGET)_elf $(TARGET).dis *.o *.bin *.map $(OBJS)
