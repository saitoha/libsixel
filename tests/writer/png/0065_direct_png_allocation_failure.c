/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <stdlib.h>
#include "stream_fault_common.h"

static unsigned char *stb_png_compressed;
static int stb_png_fail_output;
static int stb_png_output_failed;

static void *
stb_png_fault_malloc(size_t size)
{
    if (stb_png_fail_output) {
        stb_png_fail_output = 0;
        stb_png_output_failed = 1;
        errno = ENOMEM;
        return NULL;
    }
    return malloc(size);
}

static void
stb_png_fault_free(void *ptr)
{
    if (ptr != NULL && ptr == stb_png_compressed) {
        stb_png_compressed = NULL;
        free(ptr);
        /* A cleanup routine may change errno. */
        errno = EIO;
        return;
    }
    free(ptr);
}

static unsigned char *
stb_png_fault_compress(unsigned char *data, int size, int *length, int quality)
{
    (void)data;
    (void)size;
    (void)quality;
    stb_png_compressed = calloc(8u, 1u);
    *length = 8;
    /* The next allocation wraps the compressed data in the final PNG. */
    stb_png_fail_output = stb_png_compressed != NULL;
    return stb_png_compressed;
}

#define STB_IMAGE_WRITE_STATIC
#define STBIWDEF static inline
#define STBIW_MALLOC stb_png_fault_malloc
#define STBIW_REALLOC realloc
#define STBIW_FREE stb_png_fault_free
#define STBIW_ZLIB_COMPRESS stb_png_fault_compress
#define stbi_write_png_to_mem stb_png_fault_write_to_mem
#include "src/stb_image_write.c"
#undef STBIW_ZLIB_COMPRESS
#undef STBIW_FREE
#undef STBIW_REALLOC
#undef STBIW_MALLOC
#undef STB_IMAGE_WRITE_STATIC
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STBIW_REALLOC_SIZED
#undef STBIW_MEMMOVE
#undef STBIWDEF

unsigned char *
png_test_stb_fail_output_allocation(unsigned char const *pixels, int stride,
                                     int width, int height, int components,
                                     int *length)
{
    stb_png_output_failed = 0;
    return stbi_write_png_to_mem(pixels, stride, width, height, components,
                                length);
}
#undef stbi_write_png_to_mem

int
png_test_stb_released_compressed_data(void)
{
    return stb_png_output_failed && stb_png_compressed == NULL;
}

int
test_png_stream_0065(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_test_stream_fault(PNG_STREAM_DIRECT_PNG_ALLOCATION_FAILURE);
}
