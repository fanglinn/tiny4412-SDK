# tiny4412
SDK of tiny4412

# make

## u-boot

```
make tiny4412_defconfig
make all
```

## kernel

```
make uImage LOADADDR=0x40008000 -j4
```



# load

## tftp

```
//有Ramdisk
usb start;tftp 0x40080000 uImage;tftp 0x41000000 ramdisk.img;tftp 0x42000000 exynos4412-tiny4412.dtb;bootm 0x40080000 0x41000000 0x42000000

//没有Ramdisk
usb start;tftp 0x40080000 uImage;tftp 0x42000000 exynos4412-tiny4412.dtb;bootm 0x40080000 - 0x42000000
```

## nfs

```
setenv serverip 192.168.1.101
usb start;nfs 0x40600000 192.168.1.101:/home/flinn/bin/uImage;nfs 0x42000000 192.168.1.101:/home/flinn/bin/exynos4412-tiny4412.dtb;bootm 0x40600000 - 0x42000000
```



# menuconfig

## u-boot

```
Device Drivers  --->
	MMC Host controller Support  --->
		[*] Enable MMC controllers using Driver Model
			[*]   Support MMC controller operations using Driver Model
		[*] Secure Digital Host Controller Interface support
		[*]   Support SDHCI SDMA
		[*]   SDHCI support on Samsung S5P SoC 

[*] USB support  --->
	[*]   EHCI HCD (USB 2.0) support
		[*]   Support for generic EHCI USB controller
	[*]   USB Mass Storage support 
	[*]     USB keyboard polling (Poll via control EP)  ---> 
	[*]   USB Gadget Support  ---> 
	
Command line interface  ---
	[*] Fastboot support  --->
		Device access commands  --->
			[*] usb
		Network commands  --->
			[*] bootp, tftpboot (NEW)
			[*] nfs (NEW)
			[*] ping (NEW)
		Filesystem commands  --->  
			[*] ext2 command support (NEW)
			[*] FAT command support 
			[*] filesystem commands
```

## kernel

```bash
Device Drivers  --->
	[*] USB support  --->
		<*>   EHCI HCD (USB 2.0) support
			<*>     EHCI support for Samsung S5P/EXYNOS SoC Series
		<*>   USB Mass Storage support
		<*>   USB4604 HSIC to USB20 Driver

File systems  ---> 
    [*] Network File Systems  --->
        <*>   NFS client support
        
Device Drivers  --->
    <*> MMC/SD/SDIO card support  --->
        <*>   MMC block device driver (NEW)
        <*>   Synopsys DesignWare Memory Card Interface
         <*>     Exynos specific extensions for Synopsys DW Memory Card Interface
         
  Device Drivers  ---> 
	[*] LED Support  --->
		<*>   LED Class Support 
		[*]   LED Trigger support  --->
			<*>   LED Heartbeat Trigger
```

# partition

```c
SD      : mmc 2:1
eMMC    : mmc 4:1
```



## SD

```c
/*   @new one
 *    SD MMC layout:
 *    +------------------------------------------------------------------------------------------+
 *    |                                                                                          |
 *    |            |            |               |              |                |                |
 *    |   512B     |   8K(bl1)  |    16k(bl2)   |  512K(u-boot)|    92k(TZSW)   | 16K(ENV)       |
 *    |            |            |               |              |                |                |
 *    |                                                                                          |
 *    +------------------------------------------------------------------------------------------+
 *
 * signed_bl1_position=1
 * bl2_position=17
 * uboot_position=49
 * tzsw_position=1073
 * #env_position = 1257
 */
```



## eMMC

```c
/*   @new one
 *    SD MMC layout:
 *    +------------------------------------------------------------------------------------------+
 *    |                                                                                          |
 *    |            |            |               |              |                |                |
 *    |   0B       |   8K(bl1)  |    16k(bl2)   |  512K(u-boot)|    92k(TZSW)   | 16K(ENV)       |
 *    |            |            |               |              |                |                |
 *    |                                                                                          |
 *    +------------------------------------------------------------------------------------------+
 *
 * signed_bl1_position=0
 * bl2_position=16
 * uboot_position=48
 * tzsw_position=1072
 * #env_position = 1256
 *
 */
```

