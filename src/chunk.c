/*
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

#include "config.h"

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
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
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif  /* HAVE_SYS_SELECT_H */

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(HAVE_MEMMOVE)
# define memmove(d, s, n) (bcopy ((s), (d), (n)))
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

#include "chunk.h"
#include "allocator.h"


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

    status = SIXEL_OK;

end:
    return status;
}


void
sixel_chunk_destroy(
    sixel_chunk_t * const /* in */ pchunk)
{
    sixel_allocator_t *allocator;

    if (pchunk) {
        allocator = pchunk->allocator;
        sixel_allocator_free(allocator, pchunk->buffer);
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
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
static int
wait_file(int fd, int usec)
{
    int ret = 1;
#if HAVE_SYS_SELECT_H
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
# pragma GCC diagnostic pop
#endif


static SIXELSTATUS
open_binary_file(
    FILE        /* out */   **f,
    char const  /* in */    *filename)
{
    SIXELSTATUS status = SIXEL_FALSE;
#if HAVE_STAT
    struct stat sb;
#endif  /* HAVE_STAT */

    if (filename == NULL || strcmp(filename, "-") == 0) {
        /* for windows */
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(
#  if HAVE__FILENO
            _fileno(stdin),
#  else
            fileno(stdin),
#  endif  /* HAVE__FILENO */
            O_BINARY);
# elif HAVE_SETMODE
        setmode(
#  if HAVE__FILENO
            _fileno(stdin),
#  else
            fileno(stdin),
#  endif  /* HAVE__FILENO */
            O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
        *f = stdin;

        status = SIXEL_OK;
        goto end;
    }

#if HAVE_STAT
    if (stat(filename, &sb) != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message("stat() failed.");
        goto end;
    }
    if (S_ISDIR(sb.st_mode)) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message("specified path is directory.");
        goto end;
    }
#endif  /* HAVE_STAT */

    *f = fopen(filename, "rb");
    if (!*f) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message("fopen() failed.");
        goto end;
    }

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
    FILE *f = NULL;
    size_t n;
    size_t const bucket_size = 4096;

    status = open_binary_file(&f, filename);
    if (SIXEL_FAILED(status) || f == NULL) {
        goto end;
    }

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

        if (
#if HAVE__ISATTY
            _isatty(
# if HAVE__FILENO
                _fileno(f)
# else
                fileno(f)
# endif  /* HAVE__FILENO */
            )
#else
            isatty(
# if HAVE__FILENO
                _fileno(f)
# else
                fileno(f)
# endif  /* HAVE__FILENO */
            )
#endif  /* HAVE__ISATTY */
        ) {
            for (;;) {
                if (*cancel_flag) {
                    status = SIXEL_INTERRUPTED;
                    goto end;
                }
                ret = wait_file(
# if HAVE__FILENO
                    _fileno(f),
# else
                    fileno(f),
# endif  /* HAVE__FILENO */
                    10000);
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
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <winhttp.h>

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

#if HAVE_WINHTTP
static SIXELSTATUS
sixel_chunk_from_url_with_winhttp(
    char const      /* in */ *url,
    sixel_chunk_t   /* in */ *pchunk,
    int             /* in */ finsecure
)
{
    SIXELSTATUS status = SIXEL_FALSE;
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
        goto end;
    }

    wurl = utf8_to_wide(url);
    if (wurl == NULL) {
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
        goto end;
    }

    bRet = WinHttpReceiveResponse(hRequest, NULL);
    if (!bRet) {
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
                goto err;
            }
            pchunk->buffer = p;
        }

        dwRead = 0;
        if (! WinHttpReadData(hRequest,
                              pchunk->buffer + pchunk->size,
                              dwAvail, &dwRead)) {
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

    status = SIXEL_OK;

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
        status = SIXEL_CURL_ERROR & CURLE_FAILED_INIT;
        sixel_helper_set_additional_message("curl_easy_init() failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_URL, url);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR & (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_URL) failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR & (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_FOLLOWLOCATION) failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_USERAGENT,
                            "libsixel/" LIBSIXEL_VERSION);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR & (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_USERAGENT) failed.");
        goto end;
    }

    if (finsecure && strncmp(url, "https://", 8) == 0) {
        code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        if (code != CURLE_OK) {
            status = SIXEL_CURL_ERROR & (code & 0xff);
            sixel_helper_set_additional_message(
                "curl_easy_setopt(CURLOPT_SSL_VERIFYPEER) failed.");
            goto end;
        }

        code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        if (code != CURLE_OK) {
            status = SIXEL_CURL_ERROR & (code & 0xff);
            sixel_helper_set_additional_message(
                "curl_easy_setopt(CURLOPT_SSL_VERIFYHOST) failed.");
            goto end;
        }

    }

    code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR & (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed.");
        goto end;
    }

    code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pchunk);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR & (code & 0xff);
        sixel_helper_set_additional_message(
            "curl_easy_setopt(CURLOPT_WRITEDATA) failed.");
        goto end;
    }

    code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR & (code & 0xff);
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

#if !defined(HAVE_WINHTTP) && !defined(HAVE_LIBCURL)
    (void) url;
    (void) pchunk;
    (void) finsecure;
    sixel_helper_set_additional_message(
        "To specify URI schemes, you must configure this program "
        "at compile time using either the --with-libcurl option "
        "or the --with-winhttp (only available on Windows) option.\n");
    status = SIXEL_NOT_IMPLEMENTED;
#endif  /* !defined(HAVE_WINHTTP) && !defined(HAVE_LIBCURL) */

    return status;
}


SIXELSTATUS
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


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    unsigned char *ptr = malloc(16);

#ifdef HAVE_LIBCURL
    sixel_chunk_t chunk = {0, 0, 0, NULL};
    int nread;

    nread = memory_write(NULL, 1, 1, NULL);
    if (nread != 0) {
        goto error;
    }

    nread = memory_write(ptr, 1, 1, &chunk);
    if (nread != 0) {
        goto error;
    }

    nread = memory_write(ptr, 0, 1, &chunk);
    if (nread != 0) {
        goto error;
    }
#else
    nret = EXIT_SUCCESS;
    goto error;
#endif  /* HAVE_LIBCURL */
    nret = EXIT_SUCCESS;

error:
    free(ptr);
    return nret;
}


static int
test2(void)
{
    int nret = EXIT_FAILURE;
    sixel_chunk_t *chunk = NULL;
    SIXELSTATUS status = SIXEL_FALSE;

    status = sixel_chunk_new(&chunk, NULL, 0, NULL, NULL);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    status = sixel_chunk_new(NULL, NULL, 0, NULL, NULL);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test3(void)
{
    int nret = EXIT_FAILURE;
    sixel_chunk_t *chunk;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status = SIXEL_FALSE;

    sixel_debug_malloc_counter = 1;

    status = sixel_allocator_new(&allocator, sixel_bad_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_chunk_new(&chunk, "../images/map8.six", 0, NULL, allocator);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test4(void)
{
    int nret = EXIT_FAILURE;
    sixel_chunk_t *chunk;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status = SIXEL_FALSE;

    sixel_debug_malloc_counter = 2;

    status = sixel_allocator_new(&allocator, sixel_bad_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_chunk_new(&chunk, "../images/map8.six", 0, NULL, allocator);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


SIXELAPI int
sixel_chunk_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
        test3,
        test4,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
