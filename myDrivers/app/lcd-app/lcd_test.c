
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> 
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define FBIOGET_VSCREENINFO     0x4600
#define FBIOPUT_VSCREENINFO     0x4601
#define FBIOGET_FSCREENINFO     0x4602
#define FBIOGETCMAP                     0x4604
#define FBIOPUTCMAP                     0x4605
#define FBIOPAN_DISPLAY         0x4606

typedef uint32_t __u32;
typedef uint16_t __u16;

struct fb_bitfield {
        __u32 offset;                   /* beginning of bitfield        */
        __u32 length;                   /* length of bitfield           */
        __u32 msb_right;                /* != 0 : Most significant bit is */ 
                                                        /* right */ 
};

struct fb_fix_screeninfo {
        char id[16];                    /* identification string eg "TT Builtin" */
        unsigned long smem_start;       /* Start of frame buffer mem */
                                                        /* (physical address) */
        __u32 smem_len;                 /* Length of frame buffer mem */
        __u32 type;                             /* see FB_TYPE_*                */
        __u32 type_aux;                 /* Interleave for interleaved Planes */
        __u32 visual;                   /* see FB_VISUAL_*              */ 
        __u16 xpanstep;                 /* zero if no hardware panning  */
        __u16 ypanstep;                 /* zero if no hardware panning  */
        __u16 ywrapstep;                /* zero if no hardware ywrap    */
        __u32 line_length;              /* length of a line in bytes    */
        unsigned long mmio_start;       /* Start of Memory Mapped I/O   */
                                                        /* (physical address) */
        __u32 mmio_len;                 /* Length of Memory Mapped I/O  */
        __u32 accel;                    /* Indicate to driver which     */
                                                        /*  specific chip/card we have  */
        __u16 capabilities;             /* see FB_CAP_*                 */
        __u16 reserved[2];              /* Reserved for future compatibility */
};


struct fb_var_screeninfo {
        __u32 xres;                             /* visible resolution           */
        __u32 yres;
        __u32 xres_virtual;             /* virtual resolution           */
        __u32 yres_virtual;
        __u32 xoffset;                  /* offset from virtual to visible */
        __u32 yoffset;                  /* resolution                   */

        __u32 bits_per_pixel;   /* guess what                   */
        __u32 grayscale;                /* 0 = color, 1 = grayscale,    */
                                                        /* >1 = FOURCC                  */
        struct fb_bitfield red;         /* bitfield in fb mem if true color, */
        struct fb_bitfield green;       /* else only length is significant */
        struct fb_bitfield blue;
        struct fb_bitfield transp;      /* transparency                 */

        __u32 nonstd;                   /* != 0 Non standard pixel format */

        __u32 activate;                 /* see FB_ACTIVATE_*            */

        __u32 height;                   /* height of picture in mm    */
        __u32 width;                    /* width of picture in mm     */

        __u32 accel_flags;              /* (OBSOLETE) see fb_info.flags */

        /* Timing: All values in pixclocks, except pixclock (of course) */
        __u32 pixclock;                 /* pixel clock in ps (pico seconds) */
        __u32 left_margin;              /* time from sync to picture    */
        __u32 right_margin;             /* time from picture to sync    */
        __u32 upper_margin;             /* time from sync to picture    */
        __u32 lower_margin;
        __u32 hsync_len;                /* length of horizontal sync    */
        __u32 vsync_len;                /* length of vertical sync      */
        __u32 sync;                             /* see FB_SYNC_*                */
        __u32 vmode;                    /* see FB_VMODE_*               */
        __u32 rotate;                   /* angle we rotate counter clockwise */
        __u32 colorspace;               /* colorspace for FOURCC-based modes */
        __u32 reserved[4];              /* Reserved for future compatibility */
};


#if 0
static void help(void)
{
        printf("Usage: ./key <id>\n");
}
#endif

/*
bool esc = false;
static void sigint_handler(int dunno)
{
        switch (dunno) {
        case SIGINT:
                esc = true;
                printf("< Ctrl+C > Press.\n");
                break;
        default:
                break;
        }
}
*/


#define RED_COLOR565    0xFF0000
#define GREEN_COLOR565  0x00FF00
#define BLUE_COLOR565   0x0000FF

int main(int argc, char **argv)
{
        int fb = 0;
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        long int screen_size = 0;
        uint32_t *fbp = NULL;

        int x = 0, y = 0;

        fb = open("/dev/fb0", O_RDWR);
        if(!fb) {
                printf("open /dev/fb0 return error\n");
                return -1;
        }

        if(ioctl(fb, FBIOGET_FSCREENINFO, &finfo)) {
                printf("get fb fixed infomation return error\n");
                return -1;
        }

        if(ioctl(fb, FBIOGET_VSCREENINFO, &vinfo)) {
                printf("get fb variable infomation return error\n");
                return -1;
        }

        screen_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
        printf("%d-%d, %dbpp, screen_size = %ld\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, screen_size);

        fbp = (uint32_t *)mmap(0, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
        if(fbp < 0) {
                printf("mmap return error\n");
                return -1;
        }

    {  
        // Red Screen   
        printf("Red Screen\n");  
        for(y = 0; y < vinfo.yres/3;  y++)  
        {  
            for(x = 0; x < vinfo.xres ; x++)  
            {  
                *(fbp + y * vinfo.xres + x) = RED_COLOR565;  
            }  
        }  

        // Green Screen   
        printf("Green Screen\n");  
        for(y = vinfo.yres/3; y < (vinfo.yres*2)/3; y++)  
        {  
            for(x = 0; x < vinfo.xres; x++)  
            {  
                *(fbp + y * vinfo.xres + x) =GREEN_COLOR565;  
            }  
        }  

        // Blue Screen   
        printf("Blue Screen\n");  
        for(y = (vinfo.yres*2)/3; y < vinfo.yres; y++)  
        {  
            for(x = 0; x < vinfo.xres; x++)  
            {  
                *(fbp + y * vinfo.xres + x) = BLUE_COLOR565;  
            }  
        }  
    }  

        munmap(fbp, screen_size);
        close(fb);
        return 0;
}
