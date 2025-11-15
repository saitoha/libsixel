/*
 * SPDX-License-Identifier: MIT
 *
 * The CLI tools install a SIGABRT handler when the signal keeps the default
 * disposition.  The handler prints a short stack trace to stderr before
 * handing the signal back to the default action so sanitizers retain control.
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

#include "aborttrace.h"

#if defined(SIXEL_ENABLE_ABORT_TRACE)

#include <stdlib.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#if defined(HAVE_STRING_H)
#include <string.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(HAVE_PROCESS_H)
#include <process.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#if defined(HAVE_DBGHELP)
#include <dbghelp.h>
#endif
#else
#if defined(HAVE_EXECINFO_H)
#include <execinfo.h>
#endif
#endif

#define SIXEL_ABORTTRACE_STDERR_FD 2
#define SIXEL_ABORTTRACE_MAX_FRAMES 64

static volatile sig_atomic_t g_aborttrace_installed = 0;

static size_t
sixel_aborttrace_strlen(char const *text)
{
    size_t length;

    length = 0U;
    while (text[length] != '\0') {
        length += 1U;
    }

    return length;
}

#if defined(_WIN32)
static void
sixel_aborttrace_write_count(char const *text, size_t length)
{
    size_t written;

    written = 0U;
    while (written < length) {
        unsigned int chunk;
        int rc;

        if (length - written > (size_t)INT_MAX) {
            chunk = (unsigned int)INT_MAX;
        } else {
            chunk = (unsigned int)(length - written);
        }

        rc = _write(SIXEL_ABORTTRACE_STDERR_FD,
                    text + written,
                    chunk);
        if (rc <= 0) {
            break;
        }

        written += (size_t)rc;
    }
}
#else
static void
sixel_aborttrace_write_count(char const *text, size_t length)
{
    size_t written;

    written = 0U;
    while (written < length) {
        ssize_t rc;

        rc = write(SIXEL_ABORTTRACE_STDERR_FD,
                   text + written,
                   length - written);
        if (rc <= 0) {
            break;
        }

        written += (size_t)rc;
    }
}
#endif

static void
sixel_aborttrace_write_string(char const *text)
{
    size_t length;

    length = sixel_aborttrace_strlen(text);
    sixel_aborttrace_write_count(text, length);
}

#if defined(_WIN32) || !defined(HAVE_BACKTRACE_SYMBOLS_FD)
static void
sixel_aborttrace_write_newline(void)
{
    sixel_aborttrace_write_count("\n", 1U);
}

static void
sixel_aborttrace_write_pointer(void *pointer)
{
    uintptr_t value;
    unsigned int digits;
    unsigned int index;
    char buffer[2U + sizeof(void *) * 2U];
    size_t length;

    value = (uintptr_t)pointer;
    digits = (unsigned int)(sizeof(void *) * 2U);

    buffer[0] = '0';
    buffer[1] = 'x';
    length = 2U;

    for (index = 0U; index < digits; ++index) {
        unsigned int shift;
        unsigned int nibble;

        shift = (digits - 1U - index) * 4U;
        nibble = (unsigned int)((value >> shift) & 0xFU);
        buffer[length] = (char)(nibble < 10U
                                ? ('0' + (char)nibble)
                                : ('a' + (char)(nibble - 10U)));
        length += 1U;
    }

    sixel_aborttrace_write_count(buffer, length);
}

static void
sixel_aborttrace_write_unsigned(unsigned int value)
{
    char buffer[16];
    unsigned int digits;

    digits = 0U;
    do {
        unsigned int remainder;

        remainder = value % 10U;
        buffer[digits] = (char)('0' + remainder);
        digits += 1U;
        value /= 10U;
    } while (value > 0U && digits < sizeof(buffer));

    while (digits > 0U) {
        digits -= 1U;
        sixel_aborttrace_write_count(buffer + digits, 1U);
    }
}
#endif

static unsigned char
sixel_aborttrace_ascii_tolower(unsigned char value)
{
    if (value >= 'A' && value <= 'Z') {
        return (unsigned char)(value - 'A' + 'a');
    }

    return value;
}

static int
sixel_aborttrace_match_token(char const *value, char const *token)
{
    size_t index;

    if (value == NULL || token == NULL) {
        return 0;
    }

    index = 0U;
    while (value[index] != '\0' && token[index] != '\0') {
        unsigned char lhs;
        unsigned char rhs;

        lhs = sixel_aborttrace_ascii_tolower((unsigned char)value[index]);
        rhs = sixel_aborttrace_ascii_tolower((unsigned char)token[index]);
        if (lhs != rhs) {
            return 0;
        }

        index += 1U;
    }

    return value[index] == '\0' && token[index] == '\0';
}

static int
sixel_aborttrace_env_enabled(void)
{
    char const *value;
    char const *legacy;

    legacy = getenv("SIXEL_NO_ABORT_TRACE");
    if (legacy != NULL && legacy[0] != '\0') {
        if (!sixel_aborttrace_match_token(legacy, "0") &&
            !sixel_aborttrace_match_token(legacy, "false") &&
            !sixel_aborttrace_match_token(legacy, "off")) {
            return 0;
        }
    }

    value = getenv("SIXEL_ABORT_TRACE");
    if (value == NULL || value[0] == '\0' ||
        sixel_aborttrace_match_token(value, "auto")) {
        return 1;
    }

    if (sixel_aborttrace_match_token(value, "0") ||
        sixel_aborttrace_match_token(value, "false") ||
        sixel_aborttrace_match_token(value, "off")) {
        return 0;
    }

    if (sixel_aborttrace_match_token(value, "1") ||
        sixel_aborttrace_match_token(value, "true") ||
        sixel_aborttrace_match_token(value, "on") ||
        sixel_aborttrace_match_token(value, "yes")) {
        return 1;
    }

    return 1;
}

static void
sixel_aborttrace_log_banner(void)
{
    sixel_aborttrace_write_string("\nlibsixel: abort() detected\n");
    sixel_aborttrace_write_string("libsixel: stack trace follows\n");
}

static void
sixel_aborttrace_log_footer(void)
{
    sixel_aborttrace_write_string("libsixel: abort trace complete\n");
}

#if defined(_WIN32)
static int
sixel_aborttrace_sigabrt_is_default(void)
{
    void (__cdecl *previous)(int);

    previous = signal(SIGABRT, SIG_IGN);
    if (previous == SIG_ERR) {
        return 0;
    }
    signal(SIGABRT, previous);

    return previous == SIG_DFL;
}

static void
sixel_aborttrace_dump_frames(void)
{
    void *frames[SIXEL_ABORTTRACE_MAX_FRAMES];
    USHORT depth;
    unsigned int index;
    HANDLE process;
    int have_symbols;
#if defined(HAVE_DBGHELP)
    DWORD64 displacement;
    char symbol_buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO *symbol;
#endif

    depth = CaptureStackBackTrace(0,
                                  SIXEL_ABORTTRACE_MAX_FRAMES,
                                  frames,
                                  NULL);
    process = GetCurrentProcess();
    have_symbols = 0;
#if defined(HAVE_DBGHELP)
    memset(symbol_buffer, 0, sizeof(symbol_buffer));
    symbol = (SYMBOL_INFO *)symbol_buffer;
    if (SymInitialize(process, NULL, TRUE)) {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        have_symbols = 1;
    }
#endif

    if (depth == 0U) {
        sixel_aborttrace_write_string(
            "  CaptureStackBackTrace() returned no frames\n");
    }

    for (index = 0U; index < depth; ++index) {
        sixel_aborttrace_write_string("  frame ");
        sixel_aborttrace_write_unsigned(index);
        sixel_aborttrace_write_string(": ");
#if defined(HAVE_DBGHELP)
        if (have_symbols != 0) {
            DWORD64 address;

            address = (DWORD64)(uintptr_t)frames[index];
            displacement = 0U;
            if (SymFromAddr(process, address, &displacement, symbol)) {
                sixel_aborttrace_write_string(symbol->Name);
                sixel_aborttrace_write_string(" @ ");
                sixel_aborttrace_write_pointer(
                    (void *)(uintptr_t)symbol->Address);
                sixel_aborttrace_write_newline();
                continue;
            }
        }
#endif
        sixel_aborttrace_write_pointer(frames[index]);
        sixel_aborttrace_write_newline();
    }

#if defined(HAVE_DBGHELP)
    if (have_symbols != 0) {
        SymCleanup(process);
    }
#endif
}

static void __cdecl
sixel_aborttrace_signal_handler(int signum)
{
    (void)signum;

    /* DbgHelp routines are not async-signal-safe.  The handler is only used
     * for debugging when abort() is otherwise unhandled, so the trade-off is
     * acceptable and keeps sanitizers in charge of their own handlers.
     */
    sixel_aborttrace_log_banner();
    sixel_aborttrace_dump_frames();
    sixel_aborttrace_log_footer();

    _exit(3);
}

static void
sixel_aborttrace_install_platform(void)
{
    if (signal(SIGABRT, sixel_aborttrace_signal_handler) == SIG_ERR) {
        return;
    }

    g_aborttrace_installed = 1;
}
#else
static int
sixel_aborttrace_sigabrt_is_default(void)
{
    struct sigaction current;

    if (sigaction(SIGABRT, NULL, &current) != 0) {
        return 0;
    }

    return current.sa_handler == SIG_DFL;
}

static void
sixel_aborttrace_dump_frames(void)
{
#if defined(HAVE_BACKTRACE)
    void *frames[SIXEL_ABORTTRACE_MAX_FRAMES];
    int depth;

    depth = backtrace(frames,
                      (int)(sizeof(frames) / sizeof(frames[0])));
    if (depth <= 0) {
        sixel_aborttrace_write_string(
            "  backtrace() returned no frames\n");
        return;
    }

#if defined(HAVE_BACKTRACE_SYMBOLS_FD)
    /* backtrace_symbols_fd() writes to stderr.  The routine is not
     * async-signal-safe, but abort() ends the process and the handler only
     * runs when no other consumer claimed SIGABRT.
     */
    backtrace_symbols_fd(frames, depth, SIXEL_ABORTTRACE_STDERR_FD);
#else
    {
        int index;

        for (index = 0; index < depth; ++index) {
            sixel_aborttrace_write_string("  frame ");
            sixel_aborttrace_write_unsigned((unsigned int)index);
            sixel_aborttrace_write_string(": ");
            sixel_aborttrace_write_pointer(frames[index]);
            sixel_aborttrace_write_newline();
        }
    }
#endif
#else
    sixel_aborttrace_write_string(
        "  backtrace() is unavailable on this platform\n");
#endif
}

static void
sixel_aborttrace_restore_default(void)
{
    signal(SIGABRT, SIG_DFL);
    raise(SIGABRT);
}

static void
sixel_aborttrace_signal_handler(int signum, siginfo_t *info, void *context)
{
    (void)signum;
    (void)info;
    (void)context;

    /* The glibc backtrace helpers are not async-signal-safe.  We only call
     * them while the process is already aborting to aid post-mortem work.
     */
    sixel_aborttrace_log_banner();
    sixel_aborttrace_dump_frames();
    sixel_aborttrace_log_footer();

    sixel_aborttrace_restore_default();
}

static void
sixel_aborttrace_install_platform(void)
{
    struct sigaction handler;

    memset(&handler, 0, sizeof(handler));
    handler.sa_sigaction = sixel_aborttrace_signal_handler;
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = SA_SIGINFO;

    if (sigaction(SIGABRT, &handler, NULL) != 0) {
        return;
    }

    g_aborttrace_installed = 1;
}
#endif

void
sixel_aborttrace_install_if_unhandled(void)
{
    if (g_aborttrace_installed != 0) {
        return;
    }

    if (!sixel_aborttrace_env_enabled()) {
        return;
    }

    if (!sixel_aborttrace_sigabrt_is_default()) {
        return;
    }

    sixel_aborttrace_install_platform();
}

#else
void
sixel_aborttrace_install_if_unhandled(void)
{
    /* Abort tracing disabled at configure time. */
}
#endif
