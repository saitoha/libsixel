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
#ifdef HAVE_LIBFETCH
# include <fetch.h>
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


/* initialize chunk object with specified size */
static SIXELSTATUS
sixel_chunk_init(
    sixel_chunk_t * const /* in */ pchunk,
    size_t                /* in */ initial_size)
{
    SIXELSTATUS status = SIXEL_FALSE;

    pchunk->max_size = initial_size;
    pchunk->size = 0;
    pchunk->buffer
        = (unsigned char *)sixel_allocator_malloc(pchunk->allocator, pchunk->max_size);

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


SIXELAPI void
sixel_chunk_destroy(
    sixel_chunk_t * const /* in */ pchunk)
{
    sixel_allocator_t *allocator;

    if (pchunk) {
        allocator = pchunk->allocator;
        sixel_allocator_free(allocator, pchunk->buffer);
        sixel_allocator_free(allocator, pchunk->source_path);
        sixel_allocator_free(allocator, pchunk);
        sixel_allocator_unref(allocator);
    }
}


# ifdef HAVE_LIBCURL
static size_t
memory_write(void   /* in */ *ptr,
             size_t /* in */ size,
             size_t /* in */ len,
             void   /* in */ *memory)
{
    size_t nbytes = 0;
    sixel_chunk_t *chunk;

    if (ptr == NULL || memory == NULL) {
        goto end;
    }

    chunk = (sixel_chunk_t *)memory;
    if (chunk->buffer == NULL) {
        goto end;
    }

    nbytes = size * len;
    if (nbytes == 0) {
        goto end;
    }

    if (chunk->max_size <= chunk->size + nbytes) {
        do {
            chunk->max_size *= 2;
        } while (chunk->max_size <= chunk->size + nbytes);
        chunk->buffer = (unsigned char*)sixel_allocator_realloc(chunk->allocator,
                                                                chunk->buffer,
                                                                chunk->max_size);
        if (chunk->buffer == NULL) {
            nbytes = 0;
            goto end;
        }
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
    sixel_chunk_t   /* in */ *pchunk,
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
        if (pchunk->max_size - pchunk->size < bucket_size) {
            pchunk->max_size *= 2;
            pchunk->buffer = (unsigned char *)sixel_allocator_realloc(pchunk->allocator,
                                                                      pchunk->buffer,
                                                                      pchunk->max_size);
            if (pchunk->buffer == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_chunk_from_file: sixel_allocator_realloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
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
    sixel_chunk_t   /* in */ *pchunk,
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
    void *p = NULL;

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
        if (pchunk->size + dwAvail > pchunk->max_size) {
            do {
                pchunk->max_size *= 2;
            } while (pchunk->max_size < pchunk->size + dwAvail);
            p = sixel_allocator_realloc(
                pchunk->allocator, pchunk->buffer, pchunk->max_size);
            if (! p) {
                status = SIXEL_BAD_ALLOCATION;
                sixel_helper_set_additional_message("realloc failed.");
                goto err;
            }
            pchunk->buffer = p;
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
    if (pchunk->buffer) {
        free(pchunk->buffer);
    }
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
    sixel_chunk_t   /* in */ *pchunk,
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
    sixel_chunk_t   /* in */ *pchunk,
    int             /* in */ finsecure)
{
    SIXELSTATUS status = SIXEL_FALSE;
    FILE *fetch_stream = NULL;
    unsigned char bucket[4096];
    size_t nread;
    void *grown;
    int use_insecure;
    char const *saved_peer;
    char const *saved_host;
    char *saved_peer_copy;
    char *saved_host_copy;
    char message[256];

    fetch_stream = NULL;
    nread = 0;
    grown = NULL;
    use_insecure = 0;
    saved_peer = NULL;
    saved_host = NULL;
    saved_peer_copy = NULL;
    saved_host_copy = NULL;

    use_insecure = finsecure && strncmp(url, "https://", 8) == 0;
    if (use_insecure) {
        saved_peer = getenv("SSL_NO_VERIFY_PEER");
        saved_host = getenv("SSL_NO_VERIFY_HOSTNAME");
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
            "fetchGetURL() failed (code=%d, reason=%s).",
            fetchLastErrCode,
            fetchLastErrString ? fetchLastErrString : "unknown");
        sixel_helper_set_additional_message(message);
        goto end;
    }

    for (;;) {
        nread = fread(bucket, 1, sizeof(bucket), fetch_stream);
        if (nread == 0) {
            break;
        }

        if (pchunk->max_size - pchunk->size < nread) {
            do {
                pchunk->max_size *= 2;
            } while (pchunk->max_size - pchunk->size < nread);
            grown = sixel_allocator_realloc(pchunk->allocator,
                                            pchunk->buffer,
                                            pchunk->max_size);
            if (grown == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                sixel_helper_set_additional_message(
                    "sixel_allocator_realloc() failed.");
                goto end;
            }
            pchunk->buffer = (unsigned char *)grown;
        }

        memcpy(pchunk->buffer + pchunk->size, bucket, nread);
        pchunk->size += nread;
    }

    if (ferror(fetch_stream)) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "fread() failed while reading fetched stream.");
        goto end;
    }

    status = SIXEL_OK;

end:
    if (fetch_stream != NULL) {
        fclose(fetch_stream);
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
}
# endif  /* HAVE_LIBFETCH */

/* get chunk of specified resource over libcurl function */
static SIXELSTATUS
sixel_chunk_from_url(
    char const      /* in */ *url,
    sixel_chunk_t   /* in */ *pchunk,
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


SIXELAPI SIXELSTATUS
sixel_chunk_new(
    sixel_chunk_t       /* out */   **ppchunk,
    char const          /* in */    *filename,
    int                 /* in */    finsecure,
    int const           /* in */    *cancel_flag,
    sixel_allocator_t   /* in */    *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (ppchunk == NULL) {
        sixel_helper_set_additional_message(
            "sixel_chunk_new: ppchunk is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (allocator == NULL) {
        sixel_helper_set_additional_message(
            "sixel_chunk_new: allocator is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    *ppchunk = (sixel_chunk_t *)sixel_allocator_malloc(allocator, sizeof(sixel_chunk_t));
    if (*ppchunk == NULL) {
        sixel_helper_set_additional_message(
            "sixel_chunk_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /* set allocator to chunk object */
    (*ppchunk)->allocator = allocator;

    status = sixel_chunk_init(*ppchunk, 1024 * 32);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, *ppchunk);
        *ppchunk = NULL;
        goto end;
    }

    sixel_allocator_ref(allocator);

    if (filename != NULL &&
        strcmp(filename, "-") != 0 &&
        strstr(filename, "://") == NULL) {
        size_t pathlen = strlen(filename);
        (*ppchunk)->source_path =
            (char *)sixel_allocator_malloc(allocator, pathlen + 1);
        if ((*ppchunk)->source_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_chunk_new: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            sixel_chunk_destroy(*ppchunk);
            *ppchunk = NULL;
            goto end;
        }
        memcpy((*ppchunk)->source_path, filename, pathlen + 1);
    }

    if (filename != NULL && strstr(filename, "://")) {
        status = sixel_chunk_from_url(filename, *ppchunk, finsecure);
    } else {
        status = sixel_chunk_from_file(filename, *ppchunk, cancel_flag);
    }
    if (SIXEL_FAILED(status)) {
        sixel_chunk_destroy(*ppchunk);
        *ppchunk = NULL;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}



/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
