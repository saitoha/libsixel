/*
 * OpenVMS bootstrap configuration for the native VSI C build.
 *
 * This file is deliberately conservative.  It enables only C RTL headers and
 * functions that are needed by the first static-library smoke build, while
 * leaving optional image backends such as libpng, libjpeg, WebP, TIFF, GD, WIC,
 * CoreGraphics, curl, and freedesktop thumbnailing undefined.
 */

#ifndef LIBSIXEL_OPENVMS_CONFIG_H
#define LIBSIXEL_OPENVMS_CONFIG_H

#define PACKAGE "libsixel"
#define PACKAGE_NAME "libsixel"
#define PACKAGE_STRING "libsixel 1.11.0-pre"
#define PACKAGE_VERSION "1.11.0-pre"
#define PACKAGE_BUGREPORT "saitoha@me.com"
#define PACKAGE_URL ""
#define VERSION "1.11.0-pre"

#define SIXEL_FILE_MIME_OPTION ""

#define LIBSIXEL_OPENVMS 1

#define STDC_HEADERS 1

#define HAVE_ASSERT_H 1
#define HAVE_CTYPE_H 1
#define HAVE_DIRENT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FLOAT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MATH_H 1
#define HAVE_MEMORY_H 1
#define HAVE_SETJMP_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1

/*
 * OpenVMS does not provide a GNU-compatible getopt_long() header in the
 * conservative bootstrap environment.  The img2sixel converter already ships
 * converters/getopt_stub.h, so expose that fallback intentionally and keep
 * long-option parsing enabled for the native tool.
 */
#define HAVE_GETOPT_H 0
#define HAVE_GETOPT 1
#define HAVE_GETOPT_LONG 1
#define LIBSIXEL_GETOPT_STUB_USE_SYSTEM_GETOPT 1

/*
 * The first OpenVMS build avoids terminal probing and POSIX multiplexing.
 * Those paths are not required for memory-to-sixel encoding.
 */
#define HAVE_ISATTY 0
#define HAVE_SYS_SELECT_H 0
#define HAVE_TERMIOS_H 0
#define HAVE_SYS_IOCTL_H 0
#define HAVE_SYS_TTYCOM_H 0
#define HAVE_SYS_WAIT_H 0
#define HAVE_SPAWN_H 0
#define HAVE_SYS_SYSCTL_H 0
#define HAVE_SYS_PARAM_H 0
#define HAVE_IO_H 0
#define HAVE_DIRECT_H 0
#define HAVE_PROCESS_H 0

#define HAVE_MALLOC 1
#define HAVE_REALLOC 1
#define HAVE_MEMMOVE 1
#define HAVE_TOLOWER 1
#define HAVE_STAT 1
#define HAVE_OPEN 1
#define HAVE_CLEARERR 1
#define HAVE_LDIV 1
#define HAVE_CLOCK 1

/*
 * Keep thread support disabled for the bootstrap.  Leave SIXEL_ENABLE_THREADS
 * undefined so code that distinguishes `defined(SIXEL_ENABLE_THREADS)` from
 * `#if SIXEL_ENABLE_THREADS` remains on its non-threaded path.
 */

#endif /* LIBSIXEL_OPENVMS_CONFIG_H */
