#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
 
#define COLOR_RED 0x000000FF
#define COLOR_GREEN 0x0000FF00
#define COLOR_BLUE 0x00FF0000
 
 
int fdfb = -1;
 
 
struct fb_fix_screeninfo fbfix = { 0 };
struct fb_var_screeninfo fbvar = { 0 };
long screensize = 0;
 
 
int *fb32 = NULL;
 
 
int x = 0;
int y = 0;
long location = 0;
 
 
int main(int argc, char **argv)
{
	// open framebuffer device
	fdfb = open( argv[1], O_RDWR );
	if( 0 > fdfb ) {
		printf("Failure to open framebuffer device: /dev/fb0 !\n");
		exit( -1 );
	}
	printf("Success to open framebuffer device: /dev/fb0 !\n");
 
 
	// get fixed screen information
	if( ioctl( fdfb, FBIOGET_FSCREENINFO, &fbfix ) ) {
		printf("Failure to get fixed screen information !\n");
		exit( -2 );
	}
	printf("Success to get fixed screen information !\n");
 
 
	// get varible screen information
	if( ioctl( fdfb, FBIOGET_VSCREENINFO, &fbvar ) ) {
		printf("Failure to get varible screen information !\n");
		exit( -3 );
	}
	printf("Success to get varible screen information !\n");
 
 
	// calculate the number of bytes for screen size
	screensize = fbvar.xres * fbvar.yres * ( fbvar.bits_per_pixel / 8 );
	printf("sceeninfo.xres = %d, screeninfo.yres = %d, screeninfo.bits_per_pixel = %dbpp, screensize = %d !\n", fbvar.xres, fbvar.yres, fbvar.bits_per_pixel, screensize);
	 
	 
	// mmap framebuffer to process memory space
	fb32 = (int *)mmap( 0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fdfb, 0 );
	if( NULL == fb32 ) {
	printf("Failure to map framebuffer device memory to process's memory !\n");
		exit( -4 );
	}
	printf("Success to map framebuffer device memory to process's memory !\n");
	 
	 
	// draw pixel
	if( 8 == fbvar.bits_per_pixel ) {
		printf("Starting 8 bpp framebuffer test...\n");
	}
	else if( 16 == fbvar.bits_per_pixel ) {
		printf("Starting 16 bpp framebuffer test...\n");
	 
	 
	}
	else if( 24 == fbvar.bits_per_pixel ) {
		printf("Starting 24 bpp framebuffer test...\n");
	}
	else {
		printf("Supporting 32 bpp !\n");
		
		// draw red color area
		printf("Starting to show RED area !\n");
		for( y = 0; y < fbvar.yres / 3; y++ ) {
			for( x = 0; x < fbvar.xres; x++ ) {
				*( fb32 + y * fbvar.xres + x ) = COLOR_RED;
			}
		}
		 
		 
		// draw green color area
		printf("Starting to show GREEN area !\n");
		for( y = ( fbvar.yres / 3 ); y < ( fbvar.yres * 2 / 3); y++ ) {
			for( x = 0; x < fbvar.xres; x++ ) {
				*( fb32 + y * fbvar.xres + x ) = COLOR_GREEN;
			}
		}
		 
		 
		// draw blue color area 
		printf("Starting to show BLUE area !\n");
		for( y = ( fbvar.yres * 2 / 3 ); y < fbvar.yres; y++ ) {
			for( x = 0; x < fbvar.xres; x++ ) {
				*( fb32 + y * fbvar.xres + x ) = COLOR_BLUE;
			}
		}
	
	}
	 
	 
	// unmap framebuffer memory
	munmap( fb32, screensize );
	 
	 
	printf("Finished to demo to operate framebuffer !\n");
	 
	 
	// close device handle
	close( fdfb );
	 
	 
	return 0;
}
 

