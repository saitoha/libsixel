/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/writer.h"
#include "stream_fault_common.h"

#if HAVE_LIBPNG
#include <png.h>
#endif

#if HAVE_LIBPNG && HAVE_SETJMP && HAVE_LONGJMP && defined(PNG_SETJMP_SUPPORTED)
#define PNG_STREAM_FAULT_LIBPNG 1
#else
#define PNG_STREAM_FAULT_LIBPNG 0
#endif

static struct {
    int faults;
    int opened;
    int writes;
    int idat_header_written;
    int short_writes;
    int flushes;
    int closed;
    int failed_allocations;
    int backend_releases;
    int invalid;
    int live;
    void *allocations[8];
    FILE *stream;
} png_stream_probe;

static FILE *
png_stream_fault_open(char const *path, char const *mode)
{
    ++png_stream_probe.opened;
    if (strcmp(path, "png-fault.png") != 0 || strcmp(mode, "wb") != 0) {
        png_stream_probe.invalid = 1;
    }
    if (png_stream_probe.faults & PNG_STREAM_OPEN_FAILURE) {
        errno = EACCES;
        return NULL;
    }
    return png_stream_probe.stream;
}

static size_t
png_stream_fault_write(void const *bytes, size_t size, size_t count, FILE *fp)
{
    ++png_stream_probe.writes;
    if (fp != png_stream_probe.stream || png_stream_probe.closed ||
        png_stream_probe.short_writes != 0) {
        png_stream_probe.invalid = 1;
    }
    if (count != 0u &&
        ((png_stream_probe.faults & PNG_STREAM_SHORT_WRITE) ||
         png_stream_probe.idat_header_written)) {
        ++png_stream_probe.short_writes;
        errno = ENOSPC;
        return count - 1u;
    }
    /* The chunk type ends the header write in both backends. Let that
     * complete, then fail the first data write after it. */
    if ((png_stream_probe.faults & PNG_STREAM_IDAT_SHORT_WRITE) &&
        size == 1u && (count == 4u || count == 8u) &&
        memcmp((unsigned char const *)bytes + count - 4u, "IDAT", 4u) == 0) {
        png_stream_probe.idat_header_written = 1;
    }
    return count;
}

static int
png_stream_fault_flush(FILE *fp)
{
    ++png_stream_probe.flushes;
    if (fp != png_stream_probe.stream || png_stream_probe.closed) {
        png_stream_probe.invalid = 1;
    }
    if (png_stream_probe.faults & PNG_STREAM_FLUSH_FAILURE) {
        errno = ENOSPC;
        return EOF;
    }
    return 0;
}

static int
png_stream_fault_close(FILE *fp)
{
    ++png_stream_probe.closed;
    if (fp != png_stream_probe.stream) {
        png_stream_probe.invalid = 1;
    }
    if (png_stream_probe.faults & PNG_STREAM_CLOSE_FAILURE) {
        errno = EIO;
        return EOF;
    }
    return 0;
}

static void *
png_stream_fault_malloc(size_t size)
{
    size_t i;
    void *ptr;

    if ((png_stream_probe.faults & PNG_STREAM_ALLOCATION_FAILURE) &&
        png_stream_probe.opened != 0) {
        ++png_stream_probe.failed_allocations;
        return NULL;
    }

    for (i = 0u; i < sizeof(png_stream_probe.allocations) / sizeof(void *);
         ++i) {
        if (png_stream_probe.allocations[i] == NULL) {
            ptr = malloc(size);
            if (ptr != NULL) {
                png_stream_probe.allocations[i] = ptr;
                ++png_stream_probe.live;
            }
            return ptr;
        }
    }
    png_stream_probe.invalid = 1;
    return NULL;
}

static void
png_stream_fault_free(void *ptr)
{
    size_t i;

    if (ptr == NULL) {
        return;
    }
    for (i = 0u; i < sizeof(png_stream_probe.allocations) / sizeof(void *);
         ++i) {
        if (png_stream_probe.allocations[i] == ptr) {
            png_stream_probe.allocations[i] = NULL;
            --png_stream_probe.live;
            free(ptr);
            return;
        }
    }
    /* A caller allocator must never receive a buffer owned by stb. */
    png_stream_probe.invalid = 1;
}

#if PNG_STREAM_FAULT_LIBPNG
static void
png_stream_fault_destroy(png_structpp png, png_infopp info)
{
    if (*png == NULL || *info == NULL) {
        png_stream_probe.invalid = 1;
    }
    ++png_stream_probe.backend_releases;
    png_destroy_write_struct(png, info);
}
#else
/* The builtin writer receives CRT-owned bytes from stb. Supply a small
 * buffer here so fault tests need no private stb symbols from the DLL. */
static int png_stream_fault_compression_level;

static unsigned char *
png_stream_fault_compress(unsigned char *pixels, int size, int *length,
                          int quality)
{
    (void)pixels;
    (void)size;
    (void)quality;
    *length = 8;
    return calloc(8u, 1u);
}

static unsigned char *
png_stream_fault_encode(unsigned char const *pixels, int stride, int width,
                        int height, int components, int *length)
{
    (void)pixels;
    (void)stride;
    (void)width;
    (void)height;
    (void)components;
    *length = 34;
    return calloc(34u, 1u);
}

static void
png_stream_fault_free_backend(void *ptr)
{
    if (ptr != NULL) {
        ++png_stream_probe.backend_releases;
    }
    free(ptr);
}
#endif

/*
 * Compile the production writer once with a synthetic stream. The regular
 * runner still links the unmodified writer for ordinary PNG observations.
 */
#define SIXEL_PNG_IO_OPEN png_stream_fault_open
#define SIXEL_PNG_IO_WRITE png_stream_fault_write
#define SIXEL_PNG_IO_FLUSH png_stream_fault_flush
#define SIXEL_PNG_IO_CLOSE png_stream_fault_close
#define SIXEL_PNG_IO_FREE png_stream_fault_free_backend
#define SIXEL_PNG_IO_DESTROY png_stream_fault_destroy
#if !PNG_STREAM_FAULT_LIBPNG
#define stbi_zlib_compress png_stream_fault_compress
#define stbi_write_png_to_mem png_stream_fault_encode
#define stbi_write_png_compression_level png_stream_fault_compression_level
#endif
#define sixel_writer_png_error_callback png_fault_error_callback
#define sixel_writer_png_warning_callback png_fault_warning_callback
#define sixel_writer_png_io_context png_fault_io_context
#define sixel_writer_png_io_context_t png_fault_io_context_t
#define sixel_writer_png_write_callback png_fault_write_callback
#define sixel_writer_png_flush_callback png_fault_flush_callback
#define sixel_writer_png_image png_fault_image
#define sixel_writer_png_image_t png_fault_image_t
#define sixel_writer_png_emit png_fault_emit
#define png_crc32_update png_fault_crc32_update
#define png_write_u32 png_fault_write_u32
#define sixel_builtin_png_write_chunk png_fault_write_chunk
#define png_write_color png_fault_write_color
#define sixel_writer_convert_to_rgba png_fault_convert_to_rgba
#define write_png_to_file png_fault_write_to_file
#define sixel_write_image_color png_fault_write_image_color
#define sixel_helper_write_image_file png_fault_write_image_file
#include "src/writer.c"
#undef stbi_zlib_compress
#undef stbi_write_png_to_mem
#undef stbi_write_png_compression_level
#undef sixel_writer_png_error_callback
#undef sixel_writer_png_warning_callback
#undef sixel_writer_png_io_context
#undef sixel_writer_png_io_context_t
#undef sixel_writer_png_write_callback
#undef sixel_writer_png_flush_callback
#undef sixel_writer_png_image
#undef sixel_writer_png_image_t
#undef sixel_writer_png_emit
#undef png_crc32_update
#undef png_write_u32
#undef sixel_builtin_png_write_chunk
#undef png_write_color
#undef sixel_writer_convert_to_rgba
#undef write_png_to_file
#undef sixel_write_image_color
#undef sixel_helper_write_image_file

int
png_test_stream_fault(int faults)
{
    unsigned char indexed[] = {0x80};
    unsigned char rgba[] = {255, 24, 83, 151};
    unsigned char palette[] = {0, 0, 0, 255, 255, 255};
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    SIXELSTATUS expected;
    char const *diagnostic;
    char const *actual;
    int named;
    int open_failed;
    int allocation_failed;
    int before_write;
    int write_failed;
    int idat_failed;
    int flush_failed;

    named = (faults & PNG_STREAM_STDOUT) == 0;
    open_failed = (faults & PNG_STREAM_OPEN_FAILURE) != 0;
    allocation_failed = (faults & PNG_STREAM_ALLOCATION_FAILURE) != 0;
    before_write = open_failed || allocation_failed;
    idat_failed = (faults & PNG_STREAM_IDAT_SHORT_WRITE) != 0;
    write_failed = idat_failed || (faults & PNG_STREAM_SHORT_WRITE) != 0;
    flush_failed = (faults & PNG_STREAM_FLUSH_FAILURE) != 0;
    memset(&png_stream_probe, 0, sizeof(png_stream_probe));
    png_stream_probe.faults = faults;
    /* A non-stdout FILE pointer identifies a writer-owned stream. The
     * substitutes never perform I/O on either process stream. */
    png_stream_probe.stream = named ? stderr : stdout;
    status = sixel_allocator_new(&allocator, png_stream_fault_malloc, NULL,
                                  NULL, png_stream_fault_free);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }

    /* The flush probe exercises direct PNG storage. The other probes
     * exercise index expansion and indexed backend-owned resources. */
    status = png_fault_write_image_file(
        flush_failed ? rgba : indexed, 1, 1, flush_failed ? NULL : palette,
        flush_failed ? SIXEL_PIXELFORMAT_ARGB8888 : SIXEL_PIXELFORMAT_PAL1,
        named ? "png-fault.png" : "-", SIXEL_FORMAT_PNG, allocator);
    sixel_allocator_unref(allocator);
    if (open_failed) {
        expected = SIXEL_LIBC_ERROR | (EACCES & 0xff);
        /* Keep the diagnostic distinct from a call in the lexical lint. */
        diagnostic = "fopen" "() failed.";
    } else if (allocation_failed) {
        expected = SIXEL_BAD_ALLOCATION;
        diagnostic = "write_png_to_file: sixel_allocator_malloc() failed";
    } else if (write_failed) {
#if PNG_STREAM_FAULT_LIBPNG
        expected = SIXEL_PNG_ERROR;
        diagnostic = "fwrite() failed during PNG output";
#else
        expected = SIXEL_LIBC_ERROR | (ENOSPC & 0xff);
        diagnostic = "fwrite() failed.";
#endif
    } else if (flush_failed) {
        expected = SIXEL_LIBC_ERROR | (ENOSPC & 0xff);
        diagnostic = "fflush() failed.";
    } else {
        expected = SIXEL_LIBC_ERROR | (EIO & 0xff);
        diagnostic = "fclose() failed.";
    }
    actual = sixel_helper_get_additional_message();
    if (status != expected || actual == NULL ||
        strcmp(actual, diagnostic) != 0 ||
        png_stream_probe.invalid || png_stream_probe.live != 0 ||
        png_stream_probe.backend_releases != !before_write ||
        png_stream_probe.opened != named ||
        png_stream_probe.closed != (named && !open_failed) ||
        png_stream_probe.failed_allocations != allocation_failed ||
        (png_stream_probe.writes == 0) != before_write ||
        png_stream_probe.idat_header_written != idat_failed ||
        png_stream_probe.short_writes != write_failed ||
        (write_failed && !idat_failed && png_stream_probe.writes != 1) ||
        png_stream_probe.flushes != !(before_write || write_failed)) {
        fprintf(stderr,
                "PNG stream fault %d: status=0x%x expected=0x%x, "
                "message=%s, opens=%d writes=%d idat=%d short=%d "
                "flushes=%d closes=%d failed allocations=%d live=%d "
                "backend releases=%d invalid=%d\n",
                faults, (unsigned int)status, (unsigned int)expected,
                actual != NULL ? actual : "(null)", png_stream_probe.opened,
                png_stream_probe.writes, png_stream_probe.idat_header_written,
                png_stream_probe.short_writes, png_stream_probe.flushes,
                png_stream_probe.closed, png_stream_probe.failed_allocations,
                png_stream_probe.live, png_stream_probe.backend_releases,
                png_stream_probe.invalid);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#undef PNG_STREAM_FAULT_LIBPNG
