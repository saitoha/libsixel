#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "sixel.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "config.h"

enum
{
   STBI_default = 0, /* only used for req_comp */
   STBI_grey = 1,
   STBI_grey_alpha = 2,
   STBI_rgb = 3,
   STBI_rgb_alpha = 4
};

extern unsigned char *
stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

extern void
stbi_image_free(void *retval_from_stbi_load);

extern unsigned char *
make_palette(unsigned char *data, int x, int y, int n, int c);

extern unsigned char *
apply_palette(unsigned char *data,
              int width, int height, int depth,
              unsigned char *palette, int ncolors);

static int
sixel_to_png(const char *filename)
{
    unsigned char *data;
    LibSixel_ImagePtr im;
    LibSixel_OutputContext context = { putchar, puts, printf };
    int sx, sy, comp;
    int len;
    int i;
    int max;
    int n;
    FILE *fp;

    if (filename != NULL && (fp = fopen(filename, "r")) == NULL) {
	return (-1);
    }

    len = 0;
    max = 64 * 1024;

    if ((data = (uint8_t *)malloc(max)) == NULL) {
	return (-1);
    }

    for (;;) {
	if ((max - len) < 4096) {
	    max *= 2;
    	    if ((data = (uint8_t *)realloc(data, max)) == NULL)
		return (-1);
	}
	if ((n = fread(data + len, 1, 4096, fp)) <= 0)
	    break;
	len += n;
    }

    if (fp != stdout)
    	fclose(fp);

    im = LibSixel_SixelToImage(data, len);
    if (!im) {
      return 1;
    }
    (int)stbi_write_png("out.png", im->sx, im->sy,
                        STBI_rgb, im->pixels, im->sx * 3);

    LibSixel_Image_destroy(im);

    return 0;
}

int main(int argc, char *argv[])
{
    int n;
    int filecount = 1;

    sixel_to_png(argv[n]);

    return 0;
}

/* EOF */
