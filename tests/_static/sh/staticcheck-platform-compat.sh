#!/bin/sh
# Verify the platform macro ledger and representative compatibility seams.
# Policy: docs/misc/platforms/README.md
# Policy: docs/misc/platforms/openvms.md
# Policy: docs/misc/platforms/windows.md
# Policy: docs/misc/platforms/msvc.md
# Policy: docs/misc/platforms/mingw.md
# Policy: docs/misc/platforms/cygwin-msys.md
# Policy: docs/misc/platforms/emscripten.md
# Policy: docs/misc/platforms/cosmopolitan.md
# Policy: docs/misc/platforms/macos.md
# Policy: docs/misc/platforms/posix-runtimes.md
# Policy: docs/misc/platforms/haiku.md
# Policy: docs/misc/platforms/solaris.md

set -eu

src_root=$1
classification="$src_root/tests/_static/data/platform-macro-classification.tsv"
ledger="$src_root/docs/misc/platforms/README.md"
failed=0
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-platform-compat-XXXXXX")

# shellcheck disable=SC2329
cleanup()
{
    rm -rf "$tmpdir"
}

trap cleanup EXIT HUP INT TERM

fail()
{
    echo "# $*" >&2
    failed=1
}

require_fixed()
{
    pattern=$1
    path=$2

    grep -F -- "$pattern" "$src_root/$path" >/dev/null 2>&1 ||
        fail "$path is missing: $pattern"
}

echo "1..1"

test -f "$classification" || fail "macro classification is missing"

awk -F '\t' '
    /^#/ { next }
    NF != 3 { print "invalid field count: " $0; next }
    $1 !~ /^(LIBSIXEL_OPENVMS|WITH_WINPTHREAD|__[A-Za-z0-9_]+|_[A-Z][A-Za-z0-9_]*)$/ {
        print "invalid macro: " $1
    }
    $2 !~ /^(platform|compiler|architecture|language|tooling)$/ {
        print "invalid class for " $1 ": " $2
    }
    $2 == "platform" && $3 !~ /^docs\/misc\/platforms\/[A-Za-z0-9_.-]+\.md$/ {
        print "platform macro has no policy: " $1
    }
    $2 != "platform" && $3 != "-" {
        print "non-platform macro has policy: " $1
    }
    seen[$1]++ { print "duplicate macro: " $1 }
' "$classification" > "$tmpdir/classification-errors"
test ! -s "$tmpdir/classification-errors" || {
    sed 's/^/# /' "$tmpdir/classification-errors" >&2
    failed=1
}

find "$src_root/src" "$src_root/converters" "$src_root/assessment" \
    "$src_root/include" "$src_root/examples" "$src_root/fuzz" -type f \
    \( -name '*.c' -o -name '*.h' -o -name '*.m' \) \
    ! -name 'stb_image.h' ! -name 'stb_image_write.h' -exec awk '
function emit(line, count, fields, field_index) {
    gsub(/[^A-Za-z0-9_]/, " ", line)
    count = split(line, fields, /[[:space:]]+/)
    for (field_index = 1; field_index <= count; field_index++) {
        if (fields[field_index] ~ /^__/ ||
            fields[field_index] ~ /^_[A-Z]/ ||
            fields[field_index] == "LIBSIXEL_OPENVMS" ||
            fields[field_index] == "WITH_WINPTHREAD") {
            print fields[field_index]
        }
    }
}
/^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|elif|define|undef)/ {
    directive = 1
}
directive {
    emit($0)
    if ($0 !~ /\\[[:space:]]*$/) {
        directive = 0
    }
}
' {} + | LC_ALL=C sort -u > "$tmpdir/source-macros"

awk -F '\t' '!/^#/ { print $1 }' "$classification" |
    LC_ALL=C sort -u > "$tmpdir/classified-macros"
comm -23 "$tmpdir/source-macros" "$tmpdir/classified-macros" \
    > "$tmpdir/unclassified-macros"
comm -13 "$tmpdir/source-macros" "$tmpdir/classified-macros" \
    > "$tmpdir/stale-macros"
test ! -s "$tmpdir/unclassified-macros" || {
    sed 's/^/# unclassified reserved macro: /' \
        "$tmpdir/unclassified-macros" >&2
    failed=1
}
test ! -s "$tmpdir/stale-macros" || {
    sed 's/^/# classified macro is no longer used: /' \
        "$tmpdir/stale-macros" >&2
    failed=1
}

awk -F '\t' '$2 == "platform" { print $1 "\t" $3 }' \
    "$classification" > "$tmpdir/platform-macros"
tab=$(printf '\t')
while IFS="$tab" read -r macro policy; do
    test -f "$src_root/$policy" || {
        fail "$macro policy does not exist: $policy"
        continue
    }
    grep -F -- "\`$macro\`" "$src_root/$policy" >/dev/null 2>&1 ||
        fail "$policy does not mention $macro"
    policy_name=${policy##*/}
    grep -F -- "($policy_name)" "$ledger" >/dev/null 2>&1 ||
        fail "platform ledger does not link $policy"
done < "$tmpdir/platform-macros"

require_fixed '#if defined(WITH_WINPTHREAD) && WITH_WINPTHREAD' src/threading.c
require_fixed '#elif defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)' src/threading.c
require_fixed 'GetEnvironmentVariableA(name, NULL, 0)' src/compat_stub.c
require_fixed 'SetEnvironmentVariableA(name, value)' src/compat_stub.c
require_fixed 'GetConsoleMode((HANDLE)handle, &mode)' src/compat_stub.c
require_fixed 'const ULONGLONG epoch_offset = 116444736000000000ULL;' src/compat_stub.c

require_fixed 'sixel_msvc_mode=no' configure.ac
require_fixed 'AC_PROG_CC' configure.ac
msvc_line=$(awk '/^sixel_msvc_mode=no$/ { print NR; exit }' "$src_root/configure.ac")
compiler_line=$(awk '/^AC_PROG_CC$/ { print NR; exit }' "$src_root/configure.ac")
test -n "$msvc_line" && test -n "$compiler_line" &&
    test "$msvc_line" -lt "$compiler_line" ||
    fail "MSVC detection must precede AC_PROG_CC"
require_fixed 'written = _vscprintf(format, args_copy);' src/compat_stub.c
require_fixed 'msvc_result = _vsnprintf_s(buffer,' src/compat_stub.c
require_fixed 'result = _stat64i32(libc_path,' src/compat_stub.c
require_fixed 'handle = _beginthreadex(NULL, 0,' src/threading.c

require_fixed '#  define SIXEL_PRINTF_ARCHETYPE __MINGW_PRINTF_FORMAT' src/compat_stub.h
require_fixed '_CRTIMP int __cdecl _setmode(int fd, int mode);' src/compat_stub.c
require_fixed '#  define SIXEL_COMPAT_API __declspec(dllexport)' src/compat_stub.h
require_fixed "uuid = cc.find_library('uuid', required: false)" meson.build

require_fixed 'cygwin_conv_path(CCP_WIN_A_TO_POSIX, path, NULL, 0)' src/path.c
require_fixed 'cygwin_conv_path(CCP_WIN_A_TO_POSIX, path, NULL, 0)' converters/path.c
require_fixed 'static char const *clipboard_prefix = "clipboard:";' src/path.c
require_fixed 'static char const *clipboard_prefix = "clipboard:";' converters/path.c

require_fixed 'sixel_emscripten_mode=no' configure.ac
emscripten_line=$(awk '/^sixel_emscripten_mode=no$/ { print NR; exit }' \
    "$src_root/configure.ac")
test -n "$emscripten_line" && test -n "$compiler_line" &&
    test "$emscripten_line" -lt "$compiler_line" ||
    fail "Emscripten detection must precede AC_PROG_CC"
require_fixed '-sRETAIN_COMPILER_SETTINGS=1' configure.ac
require_fixed '-sNODERAWFS=1' configure.ac
require_fixed "emscripten_fetch_flag = '-sFETCH=1'" meson.build
require_fixed 'emscripten_get_compiler_setting("NODERAWFS")' src/path.c
require_fixed 'fetch = emscripten_fetch(&attr, url);' src/chunk.c
require_fixed '#if defined(O_EXCL) && !defined(__EMSCRIPTEN__)' src/decoder.c

require_fixed 'return IsWindows() ? 1 : 0;' src/path.c
require_fixed 'return IsWindows() ? 1 : 0;' converters/path.c

require_fixed "CPPFLAGS=\"\$CPPFLAGS -D_DARWIN_C_SOURCE=1\"" configure.ac
require_fixed "add_project_arguments('-D_DARWIN_C_SOURCE', language: 'c')" meson.build
require_fixed '# define _DARWIN_C_SOURCE' src/threading.c
require_fixed '# undef vsnprintf' src/compat_stub.c
require_fixed 'mib[1] = HW_AVAILCPU;' src/threading.c

require_fixed '# define _BSD_SOURCE' src/threading.c
require_fixed '# define _NETBSD_SOURCE' src/threading.c
require_fixed '# define _DRAGONFLY_SOURCE' src/threading.c
require_fixed 'fetchIO *fetch_stream = NULL;' src/chunk.c
require_fixed 'fetched = fetchIO_read(fetch_stream, bucket, sizeof(bucket));' src/chunk.c
require_fixed 'HAVE_POSIX_SPAWNP && !defined(__FreeBSD__) && !defined(__DragonFly__)' src/loader-gnome-thumbnailer.c

require_fixed '# if HAVE_EXECINFO_H' converters/aborttrace.c
require_fixed '#  if HAVE_BACKTRACE' converters/aborttrace.c
require_fixed 'int backtrace(void **buffer, int size);' converters/aborttrace.c
require_fixed '#  if HAVE_BACKTRACE_SYMBOLS_FD' converters/aborttrace.c
require_fixed 'void backtrace_symbols_fd(void *const *buffer, int size, int fd);' converters/aborttrace.c
require_fixed 'encoding = _resolve_locale_encoding(default="ascii")' python/libsixel/__init__.py
require_fixed "build_os=\"\${RUNTIME_ENV_BUILD_OS-unknown}\"" tests/loader/libwebp/0164_loader_libwebp_sigint_pipeline_stop_trace.t
require_fixed 'SIXEL_TEST_SKIP_HAIKU_PSD_TYSH_TRACE' tests/loader/builtin/1021_loader_builtin_psd_cmyk8_values_named_cmyk_trace.t
require_fixed 'meson test -C builddir --no-rebuild --num-processes 1' .github/actions/ci-steps/action.yml
require_fixed "--slice \"\${slice}/\${slice_count}\" --print-errorlogs" .github/actions/ci-steps/action.yml
require_fixed 'if pkgman refresh &&' .github/actions/ci-steps/action.yml

require_fixed 'volatile int jpeg_failed;' src/loader-libjpeg.c
require_fixed '#if HAVE_SYS_TTYCOM_H' src/tty.c
require_fixed '#if defined(TIOCGWINSZ)' src/tty.c
require_fixed "'sys/ttycom.h'," meson.build
require_fixed 'sys/ttycom.h' configure.ac
require_fixed 'strip_single_quotes()' build-aux/resolve-test-tool-paths.sh.in
require_fixed 'quote_single()' build-aux/resolve-test-tool-paths.sh.in
require_fixed 'COVERAGE_AWK="/usr/xpg4/bin/awk"' .github/actions/ci-steps/action.yml
require_fixed 'COVERAGE_AWK="nawk"' .github/actions/ci-steps/action.yml
require_fixed 'label: Autotools-solaris-11.4-x86_64' .github/workflows/ci.yml
require_fixed '--disable-dependency-tracking' .github/workflows/ci.yml
require_fixed 'make: gmake' .github/workflows/ci.yml

test "$failed" -eq 0 || {
    echo "not ok 1 - platform compatibility ledger and seams are synchronized"
    exit 1
}

echo "ok 1 - platform compatibility ledger and seams are synchronized"
exit 0
