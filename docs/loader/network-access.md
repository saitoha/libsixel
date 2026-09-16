# Network Access in img2sixel

`img2sixel` can read an image from a URL when a network binding is compiled in. The bindings are libcurl, libfetch, and Windows WinHTTP; Emscripten uses a separate Fetch API implementation through the libfetch build option. The accepted URL schemes and HTTPS capabilities therefore depend on the particular build and its dependencies. `-k` / `--insecure` requests skipping server certificate verification where the binding implements it, weakening protection against server impersonation.

```sh
# Normal HTTPS input: keep certificate verification enabled.
img2sixel 'https://images.example.org/picture.png'

# Local files do not require a network binding.
img2sixel ./picture.png
```

The hostnames in these examples are placeholders. Quote URLs so shell metacharacters such as `&` are passed as part of the URL.

## From a URL to image bytes

The shared input code in [`src/chunk.c`](../../src/chunk.c) treats an ordinary input operand containing `://` as a URL and passes it to the compiled network bindings. Other ordinary operands use the local file or standard-input path. This is a routing test, not URL validation: a string such as `data:image/png;base64,...` does not enter the URL path, and a hostname without `://` is treated as a filename. The explicit clipboard pseudo targets are handled separately by the CLI.

For each URL, `sixel_chunk_from_url()` tries the compiled bindings in this fixed order:

1. WinHTTP, if enabled.
2. libcurl, if enabled and the preceding attempt did not succeed.
3. libfetch, if enabled and the preceding attempts did not succeed.

The first successful fetch ends this sequence. Failure advances to the next compiled binding; it is not restricted to an unsupported-scheme error. Consequently, a TLS, proxy, or connection failure can also lead to another attempt with different backend defaults and trust configuration. There is no CLI selector for this network order. `-L` chooses image decoders and does not select the network binding.

The fetched bytes are buffered before being handed to the [image loader pipeline](README.md). Transport support and image-format support are separate requirements: fetching an HTTPS resource successfully does not make an HTML error page or an unsupported image format decodable. If no network binding is compiled in, URL input fails even though ordinary files and standard input remain usable. A `file://` URL also goes through this binding path; use a filesystem path when URL handling is unnecessary.

## Bindings and URL schemes

The table describes the bindings' scope, not a promise that every installed binary supports every listed scheme.

| Binding | URL schemes relevant to image input | Dependencies and limits |
| --- | --- | --- |
| libcurl | Build-dependent examples include `http://`, `https://`, `ftp://`, `ftps://`, `file://`, `sftp://`, and `scp://`. Other schemes may be present. | Schemes can be disabled when libcurl is built. HTTPS/FTPS require TLS support; SFTP/SCP require a supported SSH backend. The linked libcurl version and build determine the actual inventory. |
| Native libfetch | The FreeBSD/pkgsrc family provides HTTP, FTP, and local-file access; HTTPS requires an SSL/TLS-enabled build. Do not assume libcurl's wider scheme set. | The libfetch implementation, version, build options, and linked TLS library matter. FreeBSD and pkgsrc builds can omit OpenSSL support, leaving HTTPS unavailable. |
| WinHTTP | `http://` and `https://` only in this binding. | The adapter explicitly rejects other schemes. Windows supplies the HTTP/TLS stack, certificate trust, and system policy. A later libcurl/libfetch attempt can still handle a different scheme in a build containing several bindings. |
| Emscripten Fetch API | HTTP(S) resources subject to the host runtime's URL and access rules. | Selected through the libfetch option, but this is not BSD libfetch. Browser execution adds origin/CORS and mixed-content restrictions. Native libfetch's FTP and `file://` capabilities must not be inferred from the build-option name. |

libcurl's [`CURLOPT_URL` documentation](https://curl.se/libcurl/c/CURLOPT_URL.html) explicitly makes protocol support dependent on the library build. Its [dependency inventory](https://curl.se/docs/libs.html) distinguishes TLS libraries from SSH libraries such as libssh2; linking a TLS backend such as OpenSSL, GnuTLS, or Schannel does not by itself add SFTP. Conversely, an SSH backend does not supply HTTPS. HTTP/2 and HTTP/3 are HTTP transport versions, not additional URL schemes selected with `http2://` or `http3://`.

For libfetch, the [FreeBSD build definition](https://github.com/freebsd/freebsd-src/blob/main/lib/libfetch/Makefile) conditionally enables `WITH_SSL` and links `ssl`/`crypto`; the [pkgsrc options](https://github.com/NetBSD/pkgsrc/blob/trunk/net/libfetch/options.mk) similarly control `FETCH_WITH_OPENSSL`. These are concrete examples of why “libfetch enabled” does not guarantee HTTPS. See the [FreeBSD fetch(3) reference](https://github.com/freebsd/freebsd-src/blob/main/lib/libfetch/fetch.3) for its scheme and environment conventions.

The Emscripten path calls [`emscripten_fetch()`](https://emscripten.org/docs/api_reference/fetch.html) with a synchronous GET and memory-backed result. Its runtime restrictions remain in force; enabling the binding cannot grant browser permissions or bypass browser TLS policy.

### Redirects and backend policy

The libcurl adapter enables `CURLOPT_FOLLOWLOCATION` and leaves the allowed initial and redirected protocols at libcurl's defaults. Those are separate sets: a protocol accepted as an initial URL is not necessarily allowed as a redirect target. Current libcurl documents HTTP, HTTPS, FTP, and FTPS as its default redirect protocols, but installed versions and protocol builds can differ. See [`CURLOPT_REDIR_PROTOCOLS_STR`](https://curl.se/libcurl/c/CURLOPT_REDIR_PROTOCOLS_STR.html).

Proxy discovery, authentication, trust stores, TLS versions, and redirect handling are supplied by each backend rather than unified by `img2sixel`. For example, the WinHTTP adapter uses Windows proxy configuration and automatic proxy discovery, whereas libcurl and native libfetch can use their own environment conventions. A successful request with a standalone `curl` or `fetch` command is useful diagnostic evidence, but does not establish that `img2sixel` uses the same library or settings.

Applications that accept URLs from other users should validate schemes, destinations, and redirect targets before permitting retrieval. The current input path does not provide an application-level scheme or destination allowlist; a dependency with broader protocol support can reach more resources, including local files or internal services. TLS certificate verification authenticates a server and does not make an arbitrary destination appropriate to access. The [libcurl URL security guidance](https://curl.se/libcurl/c/CURLOPT_URL.html) describes these risks.

## Inspecting and selecting a build

Start with the binary that actually runs:

```sh
img2sixel --version
```

The `configured with` section lists `libcurl`, `libfetch`, and `WinHTTP` as `yes` or `no`. It reports compiled bindings, not their complete protocol list, their TLS backend, or which binding served a particular request. On Emscripten, `libfetch: yes` describes the Fetch API path.

| Binding | Autotools enable / disable | Meson enable / disable |
| --- | --- | --- |
| libcurl | `--with-libcurl` / `--without-libcurl` | `-Dcurl=enabled` / `-Dcurl=disabled` |
| libfetch or Emscripten Fetch | `--with-libfetch` / `--without-libfetch` | `-Dfetch=enabled` / `-Dfetch=disabled` |
| WinHTTP, Windows only | `--with-winhttp` / `--without-winhttp` | `-Dwinhttp=enabled` / `-Dwinhttp=disabled` |

The options default to automatic detection, subject to toolchain and platform restrictions. Check the configure summary and resulting binary instead of assuming a requested library was selected. To investigate one binding in isolation, build with the others disabled. Disabling only libcurl does not disable network input if another binding remains enabled. See [`configure.ac`](../../configure.ac), [`meson.build`](../../meson.build), and the [build guide](../build/README.md).

For a libcurl build, `curl --version` displays that curl executable's protocols and TLS implementation, and `curl-config --protocols` describes the installation associated with that configuration tool. Compare library paths before using either as evidence for `img2sixel`: `otool -L` on macOS, `ldd` on systems that provide it, or the package/build link metadata can identify the actual dependencies. A small diagnostic program linked to the same libcurl can query [`curl_version_info()`](https://curl.se/libcurl/c/curl_version_info.html): `protocols` is the runtime scheme list and `ssl_version` identifies TLS support. For static builds, use the build records rather than expecting a shared-library listing to show every dependency.

For libfetch, inspect the installed package's options, version, dependencies, and its own `fetch(3)` documentation. The library name alone does not identify one universal feature set. For WinHTTP, inspect the Windows version, certificate stores, and applicable TLS/proxy policy when otherwise identical URLs behave differently.

## `-k` / `--insecure` and server certificates

The encoder's insecure flag is off by default. Without `-k`, `img2sixel` does not request that server verification be disabled; the backend and its trust configuration determine the checks. In particular, inherited libfetch variables such as `SSL_NO_VERIFY_PEER` can already weaken verification even when the command line omits `-k`.

Normal HTTPS server authentication checks that the certificate is trusted and valid for the requested hostname. A private CA missing from the trust store, a self-signed certificate, an expired certificate, or a hostname mismatch can therefore prevent retrieval. `-k` requests relaxing certificate checks; it does not remove the server's certificate, provide a client certificate, or repair a TLS protocol/cipher mismatch.

```sh
# Temporary diagnostic against a controlled test endpoint.
img2sixel -k 'https://localhost:8443/picture.png'
```

### Exact binding behavior

| Binding | What the current adapter does with `-k` |
| --- | --- |
| libcurl | For an input URL beginning with the exact lowercase prefix `https://`, sets both `CURLOPT_SSL_VERIFYPEER=0` and `CURLOPT_SSL_VERIFYHOST=0`. This disables certificate-chain and hostname verification for that transfer. |
| Native libfetch | For the same exact `https://` prefix, temporarily sets `SSL_NO_VERIFY_PEER=1` and `SSL_NO_VERIFY_HOSTNAME=1` around `fetchGetURL()` and reading the response, then restores the previous environment. Which checks those variables control depends on the libfetch implementation. |
| WinHTTP | Requests ignoring an unknown CA, an invalid certificate date, and a certificate name mismatch through `WINHTTP_OPTION_SECURITY_FLAGS`. It does not request every available certificate-error exception. |
| Emscripten Fetch API | Ignores the insecure flag. `-k` cannot disable the browser/runtime's certificate checks. |

The libcurl and native libfetch prefix checks examine the original input URL. Thus `-k` does not activate their bypass for `HTTPS://...`, `ftps://...`, or an initial `http://...` URL that subsequently redirects to HTTPS. This is a current implementation limitation, not a recommendation to use mixed-case schemes or redirects for security policy. SSH host-key verification is a separate mechanism and is not configured by this option.

The libfetch environment override is process-wide while the fetch is in progress; restoring the old values afterward does not make it an isolated per-request setting for concurrent library callers. FreeBSD documents both variables, whereas the [pkgsrc manual](https://github.com/NetBSD/pkgsrc/blob/trunk/net/libfetch/files/fetch.3) documents `SSL_NO_VERIFY_PEER`; consult the installed implementation before assuming identical semantics. Microsoft documents the individual [WinHTTP security flags](https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags#winhttp-option-security-flags).

### Risk and safer remedies

Skipping verification leaves TLS encryption in use for HTTPS, but removes checks that establish who is at the other end. An attacker able to intercept the connection can impersonate the server, read requests and sensitive URL parameters, and replace the downloaded image. An encrypted connection to an impostor does not protect the original client-to-server exchange. This is the reason for libcurl's [warning against disabling certificate verification](https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html).

The replacement data also enters the image decoder. HTTPS with valid verification does not guarantee harmless image content, but bypassing verification gives an additional attacker an opportunity to supply it. Avoid treating `-k` as a permanent fix or placing it in a routine wrapper for remote images. Plain HTTP provides neither TLS encryption nor TLS server authentication, and `-k` does not change that.

Resolve the underlying cause where possible: use a hostname covered by the certificate, renew an expired certificate, fix the server's certificate chain, correct an inaccurate system clock, or install an independently authenticated private CA in the trust store used by the actual backend. Trust-store configuration is backend-specific; `img2sixel` has no common CA-file option. For a workflow needing downloader-specific certificate controls, fetch with a separately configured downloader that verifies the server, then pass the verified local file to `img2sixel`.

## Implementation and validation references

- [`src/chunk.c`](../../src/chunk.c) owns URL routing, network attempt order, buffering, and the binding-specific insecure behavior.
- [`src/encoder.c`](../../src/encoder.c) initializes and sets `finsecure`; [`src/loader.c`](../../src/loader.c) passes it into input acquisition.
- [`converters/img2sixel.c`](../../converters/img2sixel.c) and the [manual source](../../converters/img2sixel.1) own CLI help and binding availability output.

The existing [network test category](../../tests/io/network) contains local `file://` retrieval, malformed URL/error diagnostics, and a local self-signed HTTPS pair that expects rejection without `-k` and success with it. These tests are conditional on compiled bindings and supporting tools; the Emscripten cases explicitly skip native `file://` and `-k` expectations. In a build with several bindings, success does not identify which binding handled the request.

That suite does not establish an exhaustive per-backend scheme matrix, hostname/expiry/revocation checks, redirect behavior, every libfetch variant, or concurrent environment isolation. The distinctions above are derived from the adapter source and linked upstream references; platform-specific behavior requires validation on the corresponding build and runtime.
