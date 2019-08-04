# tiny4412 Layout

```c
/*
 *    SD MMC layout:
 *    +------------------------------------------------------------------------------------------+
 *    |                                                                                          |
 *    |            |            |               |              |                |                |
 *    |   512B     |   8K(bl1)  |    16k(bl2)   |  16K(env)    |    512k(u-boot)|                |
 *    |            |            |               |              |                |                |
 *    |                                                                                          |
 *    +------------------------------------------------------------------------------------------+
 *
 *
 * signed_bl1_position=1
 * bl2_position=17
 * #env_position = 49 
 * uboot_position=81
 * tzsw_position=2097
 *
 */


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
 *
 */
```



## SD

```c
#define CONFIG_SYS_MMC_ENV_DEV  (2)                     /* mmc2 in dts */
#define RESERVE_BLOCK_SIZE              (512)

#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_ENV_SIZE                 (16 << 10)      /* 16 KB */
#define BL1_SIZE                        (8 << 10)       /* 8 K reserved for BL1*/
#define BL2_SIZE                        (16 << 10)      /* 16K for BL2 */
#define TZSW_SIZE                       (92 << 10)      /* 92K for TZSW */

#define CONFIG_ENV_OFFSET               (RESERVE_BLOCK_SIZE + BL1_SIZE + BL2_SIZE )

/* U-Boot copy size from boot Media to DRAM.*/
#define COPY_BL2_SIZE           0x80000
#define BL2_START_OFFSET        ((CONFIG_ENV_OFFSET + CONFIG_ENV_SIZE)/512)
#define BL2_SIZE_BLOC_COUNT     (COPY_BL2_SIZE/512)
```

对应的sd_fuse

```bash
signed_bl1_position=1
bl2_position=17
uboot_position=81
tzsw_position=2097
```



## eMMC

```c
#define CONFIG_SYS_MMC_ENV_DEV  (4)                     /* mmc4 */
#define RESERVE_BLOCK_SIZE              (0)

#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_ENV_SIZE                 (16 << 10)      /* 16 KB */
#define BL1_SIZE                        (8 << 10)       /* 8 K reserved for BL1*/
#define BL2_SIZE                        (16 << 10)      /* 16K for BL2 */
#define TZSW_SIZE                       (92 << 10)      /* 92K for TZSW */

#define CONFIG_ENV_OFFSET               (RESERVE_BLOCK_SIZE + BL1_SIZE + BL2_SIZE + COPY_BL2_SIZE + TZSW_SIZE )

/* U-Boot copy size from boot Media to DRAM.*/
#define COPY_BL2_SIZE           0x80000     /* 512K */
#define BL2_START_OFFSET        ((RESERVE_BLOCK_SIZE + BL1_SIZE + BL2_SIZE)/512)
#define BL2_SIZE_BLOC_COUNT     (COPY_BL2_SIZE/512)
```



对应的sd_fuse.sh

```bash
signed_bl1_position=1
bl2_position=17
uboot_position=49
tzsw_position=1073
```



用SD烧写eMMC



1.格式化SD卡， 使用fdisk /dev/sdx

```bash
Command (m for help): n
Partition type:
   p   primary (1 primary, 0 extended, 3 free)
   e   extended
Select (default p): p
Partition number (1-4, default 2): 2
First sector (2048-30679039, default 2048): 20480
...
# format
t
b       #指定格式为fat
```

2.先根据sd卡的layout把u-boot编译后烧录到SD卡中，注意，由于上面分区起始从20480（留10M）开始，则这里使用sd_fuse时会使用前面10M

3.根据emmc的layout重新编译一个u-boot，编译完后把spl/tiny4412-spl.bin ， E4412_N.bl1.bin， u-boot.bin， E4412_tzsw.bin拷贝到SD卡的分区里面里面 

```bash
#mmc list
    SAMSUNG SDHCI: 2 (SD)
    EXYNOS DWMMC: 4
#mmc dev 2
    switch to partitions #0, OK
    mmc2 is current device
#fatls mmc 2
    94208   e4412_tzsw.bin 
    16384   tiny4412-spl.bin 
   465913   u-boot.bin 
     8192   e4412_n.bl1.bin 
# mmc partconf 4 1 1 1
# mmc dev 4 1
switch to partitions #1, OK
mmc4(part 1) is current device


# write u-boot.bin
# write E4412_tzsw.bin
#
#

```

write E4412_N.bl1.bin

```bash
# write E4412_N.bl1.bin
#  fatload mmc 2:1 0x50000000 E4412_N.bl1.bin
reading E4412_N.bl1.bin
8192 bytes read in 12 ms (666 KiB/s)
# mmc write 0x50000000 0 0x10
MMC write: dev # 4, block # 0, count 16 ... 16 blocks written: OK
```

write tiny4412-spl.bin

```bash
# write tiny4412-spl.bin
# fatload mmc 2:1 0x50000000 tiny4412-spl.bin
reading tiny4412-spl.bin
16384 bytes read in 14 ms (1.1 MiB/s)
# mmc write 0x50000000 0x10 0x20 
MMC write: dev # 4, block # 16, count 32 ... 32 blocks written: OK
```

write u-boot.bin

```bash
# fatload mmc 2:1 0x50000000 u-boot.bin    
reading u-boot.bin
477329 bytes read in 32 ms (14.2 MiB/s)
# mmc write 0x50000000 0x30 0x800      
MMC write: dev # 4, block # 48, count 2048 ... 2048 blocks written: OK
```

write E4412_tzsw.bin

```bash
# fatload mmc 2:1 0x50000000 E4412_tzsw.bin
reading E4412_tzsw.bin
94208 bytes read in 17 ms (5.3 MiB/s)
# mmc write 0x50000000 0x830 0xB8  
MMC write: dev # 4, block # 2096, count 184 ... 184 blocks written: OK
```

