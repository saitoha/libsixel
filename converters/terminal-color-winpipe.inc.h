/* SPDX-License-Identifier: MIT */
#ifndef IMG2SIXEL_TERMINAL_COLOR_WINPIPE_H
#define IMG2SIXEL_TERMINAL_COLOR_WINPIPE_H

/* Keep Windows imports guarded independently of the including source. */
#if defined(__CYGWIN__) || defined(__MSYS__)
#include <windows.h>
#include <winternl.h>

/* FilePipeLocalInformation is a documented NT information class (24).
 * The user-mode SDK does not declare its structure on every supported host.
 * Keep the ABI fields as Windows ULONG, not the host C unsigned long. */
typedef struct img2sixel_color_pipe_info {
    ULONG type, configuration, maximum_instances, current_instances;
    ULONG inbound_quota, readable, outbound_quota, writable, state, end;
} img2sixel_color_pipe_info_t;

static int
img2sixel_color_winpipe_space(HANDLE output, size_t *available)
{
    typedef NTSTATUS (NTAPI *query_t)(HANDLE, PIO_STATUS_BLOCK, PVOID,
                                     ULONG, FILE_INFORMATION_CLASS);
    union {
        FARPROC address;
        query_t query;
    } function;
    IO_STATUS_BLOCK io;
    img2sixel_color_pipe_info_t info;
    HMODULE module;
    NTSTATUS status;

    module = GetModuleHandleA("ntdll.dll");
    if (module == NULL) {
        return 0;
    }
    function.address = GetProcAddress(module, "NtQueryInformationFile");
    if (function.address == NULL) {
        return 0;
    }
    status = function.query(output, &io, &info, sizeof(info),
                             (FILE_INFORMATION_CLASS)24);
    if (status != 0) {
        return 0;
    }
    *available = (size_t)info.writable;
    return 1;
}

#endif /* __CYGWIN__ || __MSYS__ */
#endif /* IMG2SIXEL_TERMINAL_COLOR_WINPIPE_H */
