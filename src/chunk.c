/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2018 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#if HAVE_STDINT_H
# include <stdint.h>
#endif  /* HAVE_STDINT_H */
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#if HAVE_IO_H
# include <io.h>
#endif  /* HAVE_IO_H */
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif  /* HAVE_LIBCURL */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_LIBFETCH
# if defined(__EMSCRIPTEN__)
#  include <emscripten/fetch.h>
# else
#  include <fetch.h>
# endif
#endif  /* HAVE_LIBFETCH */
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif  /* HAVE_SYS_SELECT_H */

#if HAVE_WINHTTP
# if !defined(UNICODE)
#  define UNICODE
# endif
# if !defined(_UNICODE)
#  define _UNICODE
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
#endif

#if HAVE_WINDOWS_H
# include <windows.h>
#endif

#if HAVE_WINHTTP
# include <winhttp.h>
#endif

#include <sixel.h>

#if HAVE_WINHTTP
static void
sixel_winhttp_set_error_message(char const *context)
{
    DWORD last_error;
    char buffer[128];

    last_error = GetLastError();
    snprintf(buffer, sizeof(buffer),
             "%s failed (GetLastError=%lu).",
             context, (unsigned long) last_error);
    sixel_helper_set_additional_message(buffer);
}
#endif

/* for msvc */
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#include "stdio_stub.h"
#include "chunk.h"
#include "allocator.h"
#include "compat_stub.h"
#include "loader-common.h"
#include "sixel_atomic.h"

typedef struct sixel_chunk_storage {
    sixel_chunk_t chunk_interface;  /* IChunk dispatch header */
    sixel_atomic_u32_t ref;         /* reference counter */
    unsigned char *buffer;          /* loaded input bytes */
    size_t size;                    /* number of valid bytes in buffer */
    size_t max_size;                /* allocated byte capacity */
    char *source_path;              /* local source path, if available */
    sixel_allocator_t *allocator;   /* allocator object */
} sixel_chunk_storage_t;

static sixel_chunk_storage_t *
sixel_chunk_from_interface(sixel_chunk_t *chunk);
static sixel_chunk_storage_t const *
sixel_chunk_from_interface_const(sixel_chunk_t const *chunk);
static void
sixel_chunk_vtbl_ref(sixel_chunk_t *chunk);
static void
sixel_chunk_vtbl_unref(sixel_chunk_t *chunk);
static SIXELSTATUS
sixel_chunk_vtbl_init_source(
    sixel_chunk_t *chunk,
    sixel_chunk_source_request_t const *request);
static SIXELSTATUS
sixel_chunk_vtbl_init_memory(
    sixel_chunk_t *chunk,
    sixel_chunk_memory_request_t const *request);
static SIXELSTATUS
sixel_chunk_vtbl_get_bytes(sixel_chunk_t const *chunk,
                           sixel_chunk_bytes_view_t *view);
static char const *
sixel_chunk_vtbl_source_path(sixel_chunk_t const *chunk);

static sixel_chunk_vtbl_t const g_sixel_chunk_vtbl = {
    sixel_chunk_vtbl_ref,
    sixel_chunk_vtbl_unref,
    sixel_chunk_vtbl_init_source,
    sixel_chunk_vtbl_init_memory,
    sixel_chunk_vtbl_get_bytes,
    sixel_chunk_vtbl_source_path
};

static sixel_chunk_storage_t *
sixel_chunk_from_interface(sixel_chunk_t *chunk)
{
    return (sixel_chunk_storage_t *)(void *)chunk;
}

static sixel_chunk_storage_t const *
sixel_chunk_from_interface_const(sixel_chunk_t const *chunk)
{
    return (sixel_chunk_storage_t const *)(void const *)chunk;
}


static void
sixel_chunk_release_storage(sixel_chunk_storage_t *pchunk)
{
    if (pchunk == NULL || pchunk->allocator == NULL) {
        return;
    }

    sixel_allocator_free(pchunk->allocator, pchunk->buffer);
    sixel_allocator_free(pchunk->allocator, pchunk->source_path);
    pchunk->buffer = NULL;
    pchunk->size = 0u;
    pchunk->max_size = 0u;
    pchunk->source_path = NULL;
}

/* Initialize chunk byte storage with the specified capacity. */
static SIXELSTATUS
sixel_chunk_init(
    sixel_chunk_storage_t * const /* in */ pchunk,
    size_t                        /* in */ initial_size)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (initial_size == 0u) {
        initial_size = 1u;
    }

    sixel_chunk_release_storage(pchunk);
    pchunk->max_size = initial_size;
    pchunk->size = 0u;
    pchunk->buffer
        = (unsigned char *)sixel_allocator_malloc(pchunk->allocator,
                                                  pchunk->max_size);

    if (pchunk->buffer == NULL) {
        sixel_helper_set_additional_message(
            "sixel_chunk_init: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    pchunk->source_path = NULL;

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
sixel_chunk_reserve_capacity(
    sixel_chunk_storage_t *pchunk,
    size_t need_size,
    char const *limit_message,
    char const *realloc_message)
{
    size_t next_max_size;
    void *grown;

    if (need_size == 0) {
        return SIXEL_OK;
    }

    if (pchunk->size > SIXEL_ALLOCATE_BYTES_MAX ||
        need_size > SIXEL_ALLOCATE_BYTES_MAX - pchunk->size) {
        sixel_helper_set_additional_message(limit_message);
        return SIXEL_BAD_ALLOCATION;
    }

    if (pchunk->max_size >= pchunk->size &&
        pchunk->max_size - pchunk->size >= need_size) {
        return SIXEL_OK;
    }

    next_max_size = pchunk->max_size;
    for (;;) {
        if (next_max_size >= pchunk->size &&
            next_max_size - pchunk->size >= need_size) {
            break;
        }
        if (next_max_size > SIZE_MAX / 2u ||
            next_max_size > SIXEL_ALLOCATE_BYTES_MAX / 2u) {
            sixel_helper_set_additional_message(limit_message);
            return SIXEL_BAD_ALLOCATION;
        }
        next_max_size *= 2;
    }

    grown = sixel_allocator_realloc(pchunk->allocator,
                                    pchunk->buffer,
                                    next_max_size);
    if (grown == NULL) {
        sixel_helper_set_additional_message(realloc_message);
        return SIXEL_BAD_ALLOCATION;
    }

    pchunk->buffer = (unsigned char *)grown;
    pchunk->max_size = next_max_size;

    return SIXEL_OK;
}


# ifdef HAVE_LIBCURL
static size_t
memory_write(void   /* in */ *ptr,
             size_t /* in */ size,
             size_t /* in */ len,
             void   /* in */ *memory)
{
    size_t nbytes = 0;
    sixel_chunk_storage_t *chunk;

    if (ptr == NULL || memory == NULL) {
        goto end;
    }

    chunk = (sixel_chunk_storage_t *)memory;
    if (chunk->buffer == NULL) {
        goto end;
    }

    nbytes = size * len;
    if (nbytes == 0) {
        goto end;
    }

    if (SIXEL_FAILED(sixel_chunk_reserve_capacity(
            chunk,
            nbytes,
            "sixel_chunk_from_url_with_curl: input exceeds allocation limit.",
            "sixel_chunk_from_url_with_curl: sixel_allocator_realloc() failed."))) {
        nbytes = 0;
        goto end;
    }

    memcpy(chunk->buffer + chunk->size, ptr, nbytes);
    chunk->size += nbytes;

end:
    return nbytes;
}
# endif


#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
# endif
#endif
static int
wait_file(int fd, int usec)
{
    int ret = 1;
#if HAVE_SYS_SELECT_H && !defined(__EMSCRIPTEN__)
    fd_set rfds;
    struct timeval tv;

    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    ret = select(fd + 1, &rfds, NULL, NULL, &tv);
#else
    (void) fd;
    (void) usec;
#endif  /* HAVE_SYS_SELECT_H */
    if (ret == 0) {
        return (1);
    }
    if (ret < 0) {
        return ret;
    }

    return (0);
}
#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif

#if HAVE_WINHTTP
static wchar_t *
utf8_to_wide(char const *s) {
    wchar_t *w;
    int n;

    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        return NULL;
    }
    w = (wchar_t*) malloc(sizeof(wchar_t) * n);
    if (!w) {
        return NULL;
    }
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n)) {
         free(w);
         return NULL;
    }

    return w;
}
#endif  /* HAVE_WINHTTP */

#if 0
#if HAVE_WINDOWS_H
static SIXELSTATUS
sixel_check_input_path_windows(char const *filename)
{
    SIXELSTATUS status;
    wchar_t *wfilename;
    DWORD attrs;
    DWORD last_error;
    char message[2048];

    status = SIXEL_FALSE;
    wfilename = NULL;
    attrs = INVALID_FILE_ATTRIBUTES;
    last_error = 0;

    wfilename = utf8_to_wide(filename);
    if (wfilename == NULL) {
        sixel_helper_set_additional_message(
            "utf8_to_wide(input path) failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    attrs = GetFileAttributesW(wfilename);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        last_error = GetLastError();
        (void)sixel_compat_snprintf(message,
                                    sizeof(message),
                                    "GetFileAttributesW() for file '%s' "
                                    "failed (GetLastError=%lu).",
                                    filename,
                                    (unsigned long)last_error);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        sixel_helper_set_additional_message("specified path is directory.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (wfilename != NULL) {
        free(wfilename);
        wfilename = NULL;
    }
    return status;
}
#endif  /* HAVE_WINDOWS_H */
#endif


static int
sixel_fd_is_console(int fd)
{
    return sixel_compat_is_console(fd);
}


static SIXELSTATUS
open_binary_file(
    FILE        /* out */   **f,
    char const  /* in */    *filename)
{
    SIXELSTATUS status = SIXEL_FALSE;
#if 0
#if HAVE_STAT
    struct stat sb;
    char message[2048];
#endif  /* HAVE_STAT */
#endif

    if (filename == NULL || strcmp(filename, "-") == 0) {
        sixel_trace_topic_message("file_open",
                                  "open_binary_file: using stdin");
        /* for windows */
        (void)sixel_compat_set_binary(STDIN_FILENO);
        *f = stdin;

        status = SIXEL_OK;
        goto end;
    }

    sixel_trace_topic_message("file_open",
                              "open_binary_file: fopen begin path=%s",
                              filename);

#if 0
#if HAVE_WINDOWS_H
    /*
     * Use WinAPI metadata lookup before fopen() so missing paths are
     * rejected without relying on CRT stat-family behavior.
     */
    status = sixel_check_input_path_windows(filename);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
#endif
#endif

#if 0
#if HAVE_STAT
    if (sixel_compat_stat(filename, &sb) != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        (void)sixel_compat_snprintf(message,
                                    sizeof(message),
                                    "sixel_compat_stat() for file '%s' failed.",
                                    filename);
        sixel_helper_set_additional_message(message);
        goto end;
    }
    if (S_ISDIR(sb.st_mode)) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message("specified path is directory.");
        goto end;
    }
#endif  /* HAVE_STAT */
#endif

    *f = sixel_compat_fopen(filename, "rb");
    if (! *f) {
        sixel_trace_topic_message(
            "file_open",
            "open_binary_file: fopen failed path=%s errno=%d",
            filename,
            errno);
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message("sixel_compat_fopen() failed.");
        goto end;
    }

    sixel_trace_topic_message("file_open",
                              "open_binary_file: fopen success path=%s",
                              filename);

    status = SIXEL_OK;

end:
    return status;
}


/* get chunk date from specified local file path */
static SIXELSTATUS
sixel_chunk_from_file(
    char const      /* in */ *filename,
    sixel_chunk_storage_t /* in */ *pchunk,
    int const       /* in */ *cancel_flag
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int ret;
    int fd;
    FILE *f = NULL;
    size_t n;
    size_t const bucket_size = 4096;

    status = open_binary_file(&f, filename);
    if (SIXEL_FAILED(status) || f == NULL) {
        goto end;
    }

    fd = sixel_fileno(f);

    for (;;) {
        status = sixel_chunk_reserve_capacity(
            pchunk,
            bucket_size,
            "sixel_chunk_from_file: input exceeds allocation limit.",
            "sixel_chunk_from_file: sixel_allocator_realloc() failed.");
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (sixel_fd_is_console(fd)) {
            for (;;) {
                if (*cancel_flag) {
                    status = SIXEL_INTERRUPTED;
                    goto end;
                }
                ret = wait_file(fd, 10000);
                if (ret < 0) {
                    sixel_helper_set_additional_message(
                        "sixel_chunk_from_file: wait_file() failed.");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                if (ret == 0) {
                    break;
                }
            }
        }
        n = fread(pchunk->buffer + pchunk->size, 1, 4096, f);
        if (n == 0) {
            break;
        }
        pchunk->size += n;
    }

    if (f != stdin) {
        fclose(f);
    }

    status = SIXEL_OK;

end:
    return status;
}

#if HAVE_WINHTTP
static SIXELSTATUS
sixel_chunk_from_url_with_winhttp(
    char const      /* in */ *url,
    sixel_chunk_storage_t /* in */ *pchunk,
    int             /* in */ finsecure
)
{
    SIXELSTATUS status = SIXEL_RUNTIME_ERROR;
    unsigned long const timeout_ms = 10000;
    wchar_t *wua = NULL;
    wchar_t *wurl = NULL;
    wchar_t hostname[2048];
    wchar_t path[4096];
    WINHTTP_PROXY_INFO pinfo;
    WINHTTP_AUTOPROXY_OPTIONS apo;
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    INTERNET_PORT port;
    BOOL bRet;
    DWORD dwStatus = 0;
    DWORD dwSize = sizeof(dwStatus);
    DWORD dwFlags;
    DWORD dwRead = 0;
    DWORD dwAvail = 0;
    DWORD dwEnable = 1;
    URL_COMPONENTS uc;

    wua = utf8_to_wide("libsixel/" LIBSIXEL_VERSION);
    if (wua == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "utf8_to_wide(user agent) failed.");
        goto end;
    }

    wurl = utf8_to_wide(url);
    if (wurl == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "utf8_to_wide(url) failed.");
        goto end;
    }

    RtlZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = hostname;
    uc.dwHostNameLength = sizeof(hostname) / sizeof(hostname[0]);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = sizeof(path) / sizeof(path[0]);
    uc.lpszExtraInfo = NULL;
    uc.dwExtraInfoLength = 0;

    bRet = WinHttpCrackUrl(wurl, 0, 0, &uc);
    if (! bRet || ! uc.lpszHostName) {
        sixel_helper_set_additional_message(
            "WinHttpCrackUrl failed.");
        goto end;
    }

    if (uc.nScheme != INTERNET_SCHEME_HTTPS &&
        uc.nScheme != INTERNET_SCHEME_HTTP) {
        /* unavailable protocol */
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    port = uc.nPort;
    if (! port) {
        if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
            port = 443;
        } else if (uc.nScheme == INTERNET_SCHEME_HTTP) {
            port = 80;
        }
    }

    hSession = WinHttpOpen(wua,
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        sixel_helper_set_additional_message("WinHttpOpen failed.");
        goto end;
    }

    if (timeout_ms) {
        WinHttpSetTimeouts(
            hSession,
            timeout_ms,  /* dwResolveTimeout */
            timeout_ms,  /* dwConnectTimeout */
            timeout_ms,  /* dwSendTimeout */
            timeout_ms); /* dwReceiveTimeout */
    }

    hConnect = WinHttpConnect(hSession, uc.lpszHostName, port, 0);
    if (!hConnect) {
        sixel_helper_set_additional_message("WinHttpConnect failed.");
        goto end;
    }

    dwFlags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

    hRequest = WinHttpOpenRequest(hConnect, L"GET",
                                  uc.lpszUrlPath && *uc.lpszUrlPath ?
                                      uc.lpszUrlPath :
                                      L"/",
                                  NULL,
                                  WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  dwFlags);
    if (!hRequest) {
        sixel_helper_set_additional_message(
            "WinHttpOpenRequest failed.");
        goto end;
    }
    WinHttpSetOption(hRequest, WINHTTP_OPTION_ENABLE_FEATURE,
                     &dwEnable, sizeof(dwEnable));
    if (finsecure) {
        dwFlags = 0;
        dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
        dwFlags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        dwFlags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                         &dwFlags, sizeof(dwFlags));
    }

    RtlZeroMemory(&apo, sizeof(apo));
    apo.dwFlags |= WINHTTP_AUTOPROXY_AUTO_DETECT;
    apo.dwAutoDetectFlags |= WINHTTP_AUTO_DETECT_TYPE_DHCP;
    apo.dwAutoDetectFlags |= WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    apo.fAutoLogonIfChallenged = TRUE;

    RtlZeroMemory(&pinfo, sizeof(pinfo));
    if (WinHttpGetProxyForUrl(hSession, wurl, &apo, &pinfo)) {
        WinHttpSetOption(hRequest, WINHTTP_OPTION_PROXY,
                         &pinfo, sizeof(pinfo));
        if (pinfo.lpszProxy) {
            GlobalFree(pinfo.lpszProxy);
        }
        if (pinfo.lpszProxyBypass) {
            GlobalFree(pinfo.lpszProxyBypass);
        }
    }

    bRet = WinHttpSendRequest(hRequest,
                              WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                              WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bRet) {
        sixel_winhttp_set_error_message("WinHttpSendRequest");
        goto end;
    }

    bRet = WinHttpReceiveResponse(hRequest, NULL);
    if (!bRet) {
        sixel_winhttp_set_error_message("WinHttpReceiveResponse");
        goto end;
    }

    (void) WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &dwStatus,
            &dwSize,
            WINHTTP_NO_HEADER_INDEX);

    for (;;) {
        if (! WinHttpQueryDataAvailable(hRequest, &dwAvail)) {
            sixel_winhttp_set_error_message("WinHttpQueryDataAvailable");
            goto err;
        }
        if (dwAvail == 0) {
            break;
        }
        status = sixel_chunk_reserve_capacity(
            pchunk,
            (size_t)dwAvail,
            "sixel_chunk_from_url_with_winhttp: input exceeds allocation limit.",
            "sixel_chunk_from_url_with_winhttp: sixel_allocator_realloc() failed.");
        if (SIXEL_FAILED(status)) {
            goto err;
        }

        dwRead = 0;
        if (! WinHttpReadData(hRequest,
                              pchunk->buffer + pchunk->size,
                              dwAvail, &dwRead)) {
            sixel_winhttp_set_error_message("WinHttpReadData");
            goto err;
        }
        if (dwRead == 0) {
            break;
        }

        pchunk->size += dwRead;
    }

    status = SIXEL_OK;

    goto end;

err:
    if (status == SIXEL_OK) {
        status = SIXEL_RUNTIME_ERROR;
    }

end:
    if (hRequest) {
        WinHttpCloseHandle(hRequest);
    }
    if (hConnect) {
        WinHttpCloseHandle(hConnect);
    }
    if (hSession) {
        WinHttpCloseHandle(hSession);
    }
    if (wua) {
        free(wua);
    }
    if (wurl) {
        free(wurl);
    }

    return status;
}
# endif  /* HAVE_WINHTTP */


# if HAVE_LIBCURL
static SIXELSTATUS
sixel_chunk_from_url_with_curl(
    char const      /* in */ *url,
    sixel_chunk_storage_t /* in */ *pchunk,
    int             /* in */ finsecure)
{
    SIXELSTATUS status = SIXEL_FALSE;
    CURL *curl = NULL;
    CURLcode code;

    curl = curl_easy_init();
    if (curl == NULL) {
        status = SIXEL_CURL_ERROR | CURLE_FAILED_INIT;
        sixel_helper_set_additional_message("curl_easy_init() failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_URL, url);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR | (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_URL) failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR | (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_FOLLOWLOCATION) failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_USERAGENT,
                            "libsixel/" LIBSIXEL_VERSION);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR | (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_USERAGENT) failed.");
        goto end;
    }

    if (finsecure && strncmp(url, "https://", 8) == 0) {
        code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        if (code != CURLE_OK) {
            status = SIXEL_CURL_ERROR | (code & 0xff);
            sixel_helper_set_additional_message(
                "curl_easy_setopt(CURLOPT_SSL_VERIFYPEER) failed.");
            goto end;
        }

        code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        if (code != CURLE_OK) {
            status = SIXEL_CURL_ERROR | (code & 0xff);
            sixel_helper_set_additional_message(
                "curl_easy_setopt(CURLOPT_SSL_VERIFYHOST) failed.");
            goto end;
        }

    }

    code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR | (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pchunk);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR | (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_WRITEDATA) failed.");
        goto end;
    }

    code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR | (code & 0xff);
        sixel_helper_set_additional_message("curl_easy_perform() failed.");
        goto end;
    }

    status = SIXEL_OK;

end:
    if (curl) {
        curl_easy_cleanup(curl);
    }

    return status;
}
# endif  /* HAVE_LIBCURL */

# if HAVE_LIBFETCH
static SIXELSTATUS
sixel_chunk_from_url_with_fetch(
    char const      /* in */ *url,
    sixel_chunk_storage_t /* in */ *pchunk,
    int             /* in */ finsecure)
{
    SIXELSTATUS status = SIXEL_FALSE;
#if defined(__EMSCRIPTEN__)
    emscripten_fetch_attr_t attr;
    emscripten_fetch_t *fetch;
    size_t fetched_size;
    int http_status;
    char message[256];

    fetch = NULL;
    fetched_size = 0;
    http_status = 0;
    (void)finsecure;

    /*
     * Emscripten does not ship BSD libfetch. We route --with-libfetch through
     * the synchronous emscripten_fetch API and keep the same chunk-growth
     * behavior used by other network backends.
     */
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY
                    | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    fetch = emscripten_fetch(&attr, url);
    if (fetch == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "emscripten_fetch() failed.");
        goto end;
    }

    http_status = fetch->status;
    if (http_status != 0 && (http_status < 200 || http_status >= 400)) {
        status = SIXEL_RUNTIME_ERROR;
        (void)sixel_compat_snprintf(
            message,
            sizeof(message),
            "emscripten_fetch() failed (HTTP status=%d).",
            http_status);
        sixel_helper_set_additional_message(message);
        goto end;
    }

    if (fetch->numBytes < 0) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "emscripten_fetch() returned negative size.");
        goto end;
    }

    fetched_size = (size_t)fetch->numBytes;
    if (fetched_size == 0) {
        status = SIXEL_OK;
        goto end;
    }

    status = sixel_chunk_reserve_capacity(
        pchunk,
        fetched_size,
        "sixel_chunk_from_url_with_libfetch: input exceeds allocation limit.",
        "sixel_chunk_from_url_with_libfetch: sixel_allocator_realloc() failed.");
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    memcpy(pchunk->buffer + pchunk->size, fetch->data, fetched_size);
    pchunk->size += fetched_size;
    status = SIXEL_OK;

end:
    if (fetch != NULL) {
        emscripten_fetch_close(fetch);
    }
    return status;
#else
#if defined(__NetBSD__)
    fetchIO *fetch_stream = NULL;
#else
    FILE *fetch_stream = NULL;
#endif
    unsigned char bucket[4096];
    size_t nread;
#if defined(__NetBSD__)
    ssize_t fetched;
#endif
    int use_insecure;
    char const *saved_peer;
    char const *saved_host;
    char *saved_peer_copy;
    char *saved_host_copy;
    char message[256];

    fetch_stream = NULL;
    nread = 0;
    use_insecure = 0;
    saved_peer = NULL;
    saved_host = NULL;
    saved_peer_copy = NULL;
    saved_host_copy = NULL;

    use_insecure = finsecure && strncmp(url, "https://", 8) == 0;
    if (use_insecure) {
        saved_peer = sixel_compat_getenv("SSL_NO_VERIFY_PEER");
        saved_host = sixel_compat_getenv("SSL_NO_VERIFY_HOSTNAME");
        if (saved_peer != NULL) {
            saved_peer_copy = strdup(saved_peer);
            if (saved_peer_copy == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                sixel_helper_set_additional_message(
                    "strdup(SSL_NO_VERIFY_PEER) failed.");
                goto end;
            }
        }
        if (saved_host != NULL) {
            saved_host_copy = strdup(saved_host);
            if (saved_host_copy == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                sixel_helper_set_additional_message(
                    "strdup(SSL_NO_VERIFY_HOSTNAME) failed.");
                goto end;
            }
        }
        if (setenv("SSL_NO_VERIFY_PEER", "1", 1) != 0
            || setenv("SSL_NO_VERIFY_HOSTNAME", "1", 1) != 0) {
            status = SIXEL_LIBC_ERROR | (errno & 0xff);
            sixel_helper_set_additional_message(
                "setenv() for SSL_NO_VERIFY_* failed.");
            goto end;
        }
    }

    fetch_stream = fetchGetURL(url, "");
    if (fetch_stream == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        (void)sixel_compat_snprintf(
            message,
            sizeof(message),
            "fetchGetURL() failed (code=%d).",
            fetchLastErrCode);
        sixel_helper_set_additional_message(message);
        goto end;
    }

    for (;;) {
#if defined(__NetBSD__)
        fetched = fetchIO_read(fetch_stream, bucket, sizeof(bucket));
        if (fetched < 0) {
            status = SIXEL_RUNTIME_ERROR;
            sixel_helper_set_additional_message(
                "fetchIO_read() failed while reading fetched stream.");
            goto end;
        }
        if (fetched == 0) {
            break;
        }
        nread = (size_t)fetched;
#else
        nread = fread(bucket, 1, sizeof(bucket), fetch_stream);
        if (nread == 0) {
            break;
        }
#endif

        status = sixel_chunk_reserve_capacity(
            pchunk,
            nread,
            "sixel_chunk_from_url_with_libfetch: input exceeds allocation limit.",
            "sixel_chunk_from_url_with_libfetch: sixel_allocator_realloc() failed.");
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        memcpy(pchunk->buffer + pchunk->size, bucket, nread);
        pchunk->size += nread;
    }

#if !defined(__NetBSD__)
    if (ferror(fetch_stream)) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "fread() failed while reading fetched stream.");
        goto end;
    }
#endif

    status = SIXEL_OK;

end:
    if (fetch_stream != NULL) {
#if defined(__NetBSD__)
        fetchIO_close(fetch_stream);
#else
        fclose(fetch_stream);
#endif
    }
    if (use_insecure) {
        if (saved_peer_copy != NULL) {
            (void)setenv("SSL_NO_VERIFY_PEER", saved_peer_copy, 1);
        } else {
            (void)unsetenv("SSL_NO_VERIFY_PEER");
        }
        if (saved_host_copy != NULL) {
            (void)setenv("SSL_NO_VERIFY_HOSTNAME", saved_host_copy, 1);
        } else {
            (void)unsetenv("SSL_NO_VERIFY_HOSTNAME");
        }
    }
    free(saved_peer_copy);
    free(saved_host_copy);

    return status;
#endif  /* defined(__EMSCRIPTEN__) */
}
# endif  /* HAVE_LIBFETCH */

/* get chunk of specified resource over libcurl function */
static SIXELSTATUS
sixel_chunk_from_url(
    char const      /* in */ *url,
    sixel_chunk_storage_t /* in */ *pchunk,
    int             /* in */ finsecure)
{
    SIXELSTATUS status = SIXEL_NOT_IMPLEMENTED;
#if HAVE_WINHTTP
    status = sixel_chunk_from_url_with_winhttp(url, pchunk, finsecure);
    if (SIXEL_SUCCEEDED(status)) {
        return status;
    }
#endif  /* HAVE_WINHTTP */

#if HAVE_LIBCURL
    status = sixel_chunk_from_url_with_curl(url, pchunk, finsecure);
    if (SIXEL_SUCCEEDED(status)) {
        return status;
    }
#endif  /* HAVE_LIBCURL */

#if HAVE_LIBFETCH
    status = sixel_chunk_from_url_with_fetch(url, pchunk, finsecure);
    if (SIXEL_SUCCEEDED(status)) {
        return status;
    }
#endif  /* HAVE_LIBFETCH */

#if !defined(HAVE_WINHTTP) && !defined(HAVE_LIBCURL) && !defined(HAVE_LIBFETCH)
    (void) url;
    (void) pchunk;
    (void) finsecure;
    sixel_helper_set_additional_message(
        "To specify URI schemes, you must configure this program "
        "at compile time using one of --with-libcurl, --with-libfetch, "
        "or --with-winhttp (only available on Windows).\n");
    status = SIXEL_NOT_IMPLEMENTED;
#endif  /* !defined(HAVE_WINHTTP) && !defined(HAVE_LIBCURL) && !defined(HAVE_LIBFETCH) */

    return status;
}


static SIXELSTATUS
sixel_chunk_copy_source_path(sixel_chunk_storage_t *pchunk,
                             char const *source_path)
{
    size_t pathlen;
    char *copy;

    pathlen = 0u;
    copy = NULL;

    if (pchunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (source_path == NULL) {
        return SIXEL_OK;
    }

    pathlen = strlen(source_path);
    copy = (char *)sixel_allocator_malloc(pchunk->allocator, pathlen + 1u);
    if (copy == NULL) {
        sixel_helper_set_additional_message(
            "sixel_chunk_copy_source_path: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(copy, source_path, pathlen + 1u);
    sixel_allocator_free(pchunk->allocator, pchunk->source_path);
    pchunk->source_path = copy;

    return SIXEL_OK;
}

static void
sixel_chunk_vtbl_ref(sixel_chunk_t *chunk)
{
    sixel_chunk_storage_t *storage;

    storage = NULL;
    if (chunk == NULL) {
        return;
    }

    storage = sixel_chunk_from_interface(chunk);
    (void)sixel_atomic_fetch_add_u32(&storage->ref, 1u);
}

static void
sixel_chunk_vtbl_unref(sixel_chunk_t *chunk)
{
    sixel_chunk_storage_t *storage;
    sixel_allocator_t *allocator;
    unsigned int previous;

    storage = NULL;
    allocator = NULL;
    previous = 0u;
    if (chunk == NULL) {
        return;
    }

    storage = sixel_chunk_from_interface(chunk);
    previous = sixel_atomic_fetch_sub_u32(&storage->ref, 1u);
    if (previous != 1u) {
        return;
    }

    allocator = storage->allocator;
    sixel_chunk_release_storage(storage);
    storage->allocator = NULL;
    if (allocator != NULL) {
        sixel_allocator_free(allocator, storage);
        sixel_allocator_unref(allocator);
    }
}

static SIXELSTATUS
sixel_chunk_vtbl_init_source(
    sixel_chunk_t *chunk,
    sixel_chunk_source_request_t const *request)
{
    SIXELSTATUS status;
    sixel_chunk_storage_t *storage;
    int local_cancel_flag;
    int const *cancel_flag;

    status = SIXEL_FALSE;
    storage = NULL;
    local_cancel_flag = 0;
    cancel_flag = NULL;

    if (chunk == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_chunk_from_interface(chunk);
    if (storage->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_chunk_init(storage, 1024u * 32u);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (request->filename != NULL &&
        strcmp(request->filename, "-") != 0 &&
        strstr(request->filename, "://") == NULL) {
        status = sixel_chunk_copy_source_path(storage, request->filename);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    cancel_flag = request->cancel_flag;
    if (cancel_flag == NULL) {
        cancel_flag = &local_cancel_flag;
    }

    if (request->filename != NULL && strstr(request->filename, "://")) {
        status = sixel_chunk_from_url(request->filename,
                                      storage,
                                      request->finsecure);
    } else {
        status = sixel_chunk_from_file(request->filename,
                                       storage,
                                       cancel_flag);
    }
    if (SIXEL_FAILED(status)) {
        sixel_chunk_release_storage(storage);
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_chunk_vtbl_init_memory(
    sixel_chunk_t *chunk,
    sixel_chunk_memory_request_t const *request)
{
    SIXELSTATUS status;
    sixel_chunk_storage_t *storage;
    unsigned char *buffer;
    char *source_path;
    size_t capacity;
    size_t pathlen;

    status = SIXEL_FALSE;
    storage = NULL;
    buffer = NULL;
    source_path = NULL;
    capacity = 0u;
    pathlen = 0u;

    if (chunk == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->bytes == NULL && request->size != 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_chunk_from_interface(chunk);
    if (storage->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    capacity = request->size != 0u ? request->size : 1u;
    if (capacity > SIXEL_ALLOCATE_BYTES_MAX) {
        sixel_helper_set_additional_message(
            "sixel_chunk_vtbl_init_memory: input exceeds allocation limit.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    buffer = (unsigned char *)sixel_allocator_malloc(storage->allocator,
                                                    capacity);
    if (buffer == NULL) {
        sixel_helper_set_additional_message(
            "sixel_chunk_vtbl_init_memory: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (request->size != 0u) {
        memcpy(buffer, request->bytes, request->size);
    }

    if (request->source_path != NULL) {
        pathlen = strlen(request->source_path);
        source_path = (char *)sixel_allocator_malloc(storage->allocator,
                                                     pathlen + 1u);
        if (source_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_chunk_vtbl_init_memory: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(source_path, request->source_path, pathlen + 1u);
    }

    sixel_chunk_release_storage(storage);
    storage->buffer = buffer;
    storage->size = request->size;
    storage->max_size = capacity;
    storage->source_path = source_path;
    buffer = NULL;
    source_path = NULL;
    status = SIXEL_OK;

end:
    if (storage != NULL) {
        sixel_allocator_free(storage->allocator, buffer);
        sixel_allocator_free(storage->allocator, source_path);
    }
    return status;
}

static SIXELSTATUS
sixel_chunk_vtbl_get_bytes(sixel_chunk_t const *chunk,
                           sixel_chunk_bytes_view_t *view)
{
    sixel_chunk_storage_t const *storage;

    storage = NULL;
    if (chunk == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_chunk_from_interface_const(chunk);
    view->bytes = storage->buffer;
    view->size = storage->size;

    return SIXEL_OK;
}

static char const *
sixel_chunk_vtbl_source_path(sixel_chunk_t const *chunk)
{
    sixel_chunk_storage_t const *storage;

    storage = NULL;
    if (chunk == NULL) {
        return NULL;
    }

    storage = sixel_chunk_from_interface_const(chunk);
    return storage->source_path;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_chunk_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_chunk_storage_t *storage;

    storage = NULL;
    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *object = NULL;

    storage = (sixel_chunk_storage_t *)
        sixel_allocator_malloc(allocator, sizeof(*storage));
    if (storage == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    storage->chunk_interface.vtbl = &g_sixel_chunk_vtbl;
    storage->ref = 1u;
    storage->buffer = NULL;
    storage->size = 0u;
    storage->max_size = 0u;
    storage->source_path = NULL;
    storage->allocator = allocator;
    sixel_allocator_ref(allocator);

    *object = &storage->chunk_interface;
    return SIXEL_OK;
}



/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
