#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>

inline static unsigned short int make16color(unsigned char r, unsigned char g, unsigned char b)
{
    return (
            (((r >> 3) & 31) << 11) |
            (((g >> 2) & 63) << 5)  |
            ((b >> 3) & 31)        );
}

int main() {
    int fbfd = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    char *fbp = 0;
    int x = 0, y = 0;
    int guage_height = 20, step = 10;
    long int location = 0;

    // Open the file for reading and writing
    fbfd = open("/dev/graphics/fb0", O_RDWR);
    if (!fbfd) {
        printf("Error: cannot open framebuffer device.\n");
        return 0;
    }
    printf("The framebuffer device was opened successfully.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
        printf("Error reading fixed information.\n");
        return 0;
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        printf("Error reading variable information.\n");
        return 0;
    }

    printf("sizeof(unsigned short) = %d\n", sizeof(unsigned short));
    printf("%dx%d, %dbpp smem_len %d\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.smem_len);
    printf("xoffset:%d, yoffset:%d, line_length: %d\n", vinfo.xoffset, vinfo.yoffset, finfo.line_length );

    // Map the device to memory
    fbp = (char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED,
            fbfd, 0);

    if ((int)fbp == -1) {
        printf("Error: failed to map framebuffer device to memory.\n");
        return 0;
    }
    printf("The framebuffer device was mapped to memory successfully.\n");

    // Figure out where in memory to put the pixel
    for (x = step; x < vinfo.xres - step; x++ ) {
        for (y = (vinfo.yres - guage_height) / 2; y < (vinfo.yres + guage_height) / 2; y++ ) {
            location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                (y + vinfo.yoffset) * finfo.line_length;

            if ( vinfo.bits_per_pixel == 32 ) {
                *(fbp + location) = 100;        // Some blue
                *(fbp + location + 1) = 15 + (x - 100)/2;     // A little green
                *(fbp + location + 2) = 200 - (y - 100)/5;    // A lot of red
                *(fbp + location + 3) = 0;      // No transparency
            } else { //assume 16bpp
                unsigned char b = 255 * x / (vinfo.xres - step);
                unsigned char g = 255;     // (x - 100)/6 A little green
                unsigned char r = 255;    // A lot of red
                unsigned short int t = make16color(r, g, b);
                *((unsigned short int*)(fbp + location)) = t;
            }
        }

        // Get variable screen information
        if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
            printf("Error reading variable information.\n");
            return 0;
        }
        //printf("x = %d, temp = %d\n", x, temp);
        //sleep to see it
        usleep(200000);
        printf("draw x %d max_x %d\n", x, vinfo.xres - step);
    }
    //clean framebuffer
    munmap(fbp, finfo.smem_len);
    close(fbfd);

    return 0;
}
