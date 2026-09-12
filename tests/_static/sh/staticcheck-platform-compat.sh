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
# Policy: docs/misc/platforms/gnu-hurd.md
# Policy: docs/misc/platforms/macos.md
# Policy: docs/misc/platforms/posix-runtimes.md
# Policy: docs/misc/platforms/haiku.md
# Policy: docs/misc/platforms/solaris.md
# Coverage: PL-01 PL-02 PL-03 PL-04 PL-05 OV-06 WIN-01 WIN-02 MSVC-01
# Coverage: MSVC-02
# Coverage: MW-01 MW-02 CYG-01 CYG-02 EM-01 EM-02 COSMO-01
# Coverage: HURD-01 HURD-02
# Coverage: MAC-01 MAC-02 POSIX-01 POSIX-02 HAIKU-01 HAIKU-02
# Coverage: SOL-01 SOL-02 SOL-03 SOL-04

set -eu

src_root=$1
classification="$src_root/tests/_static/data/platform-macro-classification.tsv"
non_c_classification="$src_root/tests/_static/data/platform-non-c-classification.tsv"
native_windows_inventory="$src_root/tests/_static/data/native-windows-discriminator.tsv"
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
test -f "$non_c_classification" ||
    fail "non-C platform classification is missing"
test -f "$native_windows_inventory" ||
    fail "native Windows discriminator inventory is missing"

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

# Non-C compatibility selectors cannot be inferred from preprocessor spelling.
# Keep their owning source and policy explicit so shell, CI, and harness rules
# participate in the same bidirectional audit as C macros.
awk -F '\t' '
    /^#/ { next }
    NF != 4 { print "invalid field count: " $0; next }
    $2 !~ /^(build-driver|build-tool|package-tool|runtime-launch|shell-tool|test-harness)$/ {
        print "invalid non-C class for " $1 ": " $2
    }
    $3 !~ /^docs\/misc\/platforms\/[A-Za-z0-9_.-]+\.md$/ {
        print "non-C marker has no platform policy: " $1
    }
    seen[$1 "\t" $4]++ {
        print "duplicate non-C marker/source pair: " $1 " -> " $4
    }
' "$non_c_classification" > "$tmpdir/non-c-errors"
test ! -s "$tmpdir/non-c-errors" || {
    sed 's/^/# /' "$tmpdir/non-c-errors" >&2
    failed=1
}
tab=$(printf '\t')
while IFS="$tab" read -r marker class policy source; do
    case "$marker" in
      ''|'#'*) continue ;;
    esac
    : "$class"
    test -f "$src_root/$policy" || {
        fail "$marker policy does not exist: $policy"
        continue
    }
    test -f "$src_root/$source" || {
        fail "$marker source does not exist: $source"
        continue
    }
    grep -F -- "$marker" "$src_root/$policy" >/dev/null 2>&1 ||
        fail "$policy does not mention non-C marker: $marker"
    grep -F -- "$marker" "$src_root/$source" >/dev/null 2>&1 ||
        fail "$source does not retain non-C marker: $marker"
done < "$non_c_classification"

# Every platform coverage row must identify the exact assertion-bearing test,
# not merely a file with a broad reciprocal policy link.
find "$src_root/docs/misc/platforms" -type f -name '*.md' -exec awk '
    $0 == "<!-- test-coverage: enforced -->" { enforced=1; next }
    enforced != 0 && $0 ~ /^\| [A-Z][A-Z0-9]*-[0-9]+ \|/ {
        split($0, fields, "|")
        coverage=fields[2]
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", coverage)
        line=$0
        while (match(line,
                     /\[tests\/[A-Za-z0-9_.\/-]+\]\([^()[:space:]]+\)/)) {
            token=substr(line, RSTART, RLENGTH)
            separator=index(token, "](")
            test_path=substr(token, 2, separator - 2)
            print coverage "|" test_path
            line=substr(line, RSTART + RLENGTH)
        }
    }
' {} + | LC_ALL=C sort -u > "$tmpdir/doc-coverage-pairs"

find "$src_root/tests" -type f \
    \( -name '*.t' -o -name '*.c' -o -name '*.sh' \) -exec awk \
    -v src_root="$src_root/" '
    /^[[:space:]]*(#|\/\/|\*)[[:space:]]*Coverage:/ {
        line=$0
        sub(/^[[:space:]]*(#|\/\/|\*)[[:space:]]*Coverage:[[:space:]]*/, "", line)
        count=split(line, fields, /[[:space:]]+/)
        test_path=substr(FILENAME, length(src_root) + 1)
        for (field_index=1; field_index <= count; field_index++) {
            if (fields[field_index] ~ /^[A-Z][A-Z0-9]*-[0-9]+$/) {
                print fields[field_index] "|" test_path
            }
        }
    }
' {} + | LC_ALL=C sort -u > "$tmpdir/test-coverage-pairs"
comm -23 "$tmpdir/doc-coverage-pairs" "$tmpdir/test-coverage-pairs" \
    > "$tmpdir/missing-coverage-assertions"
comm -13 "$tmpdir/doc-coverage-pairs" "$tmpdir/test-coverage-pairs" \
    > "$tmpdir/extra-coverage-assertions"
test ! -s "$tmpdir/missing-coverage-assertions" || {
    sed 's/^/# platform coverage row lacks test assertion: /' \
        "$tmpdir/missing-coverage-assertions" >&2
    failed=1
}
test ! -s "$tmpdir/extra-coverage-assertions" || {
    sed 's/^/# platform test assertion lacks coverage row: /' \
        "$tmpdir/extra-coverage-assertions" >&2
    failed=1
}

# Count complete native-Windows preprocessor directives by owning file. This
# catches a single copy losing the Cygwin/MSYS exclusions or WITH_WINPTHREAD
# boundary while visually similar copies remain elsewhere.
find "$src_root/src" "$src_root/converters" "$src_root/assessment" \
    "$src_root/include" "$src_root/examples" "$src_root/fuzz" -type f \
    \( -name '*.c' -o -name '*.h' -o -name '*.m' \) -exec awk \
    -v src_root="$src_root/" '
function inspect(text, flat) {
    flat=text
    gsub(/[[:space:]]+/, " ", flat)
    if (flat ~ /defined\(_WIN32\)/ &&
        flat ~ /!defined\(__CYGWIN__\)/ &&
        flat ~ /!defined\(__MSYS__\)/) {
        counts[FILENAME]++
    }
}
/^[[:space:]]*#[[:space:]]*(if|elif)/ {
    directive=$0
    active=1
    if ($0 !~ /\\[[:space:]]*$/) {
        inspect(directive)
        active=0
    }
    next
}
active != 0 {
    directive=directive " " $0
    if ($0 !~ /\\[[:space:]]*$/) {
        inspect(directive)
        active=0
    }
}
END {
    for (path in counts) {
        relative=substr(path, length(src_root) + 1)
        print relative "\t" counts[path]
    }
}
' {} + | LC_ALL=C sort > "$tmpdir/native-windows-actual"
awk -F '\t' '!/^#/ { print }' "$native_windows_inventory" |
    LC_ALL=C sort > "$tmpdir/native-windows-expected"
cmp -s "$tmpdir/native-windows-expected" \
    "$tmpdir/native-windows-actual" || {
    echo "# native Windows discriminator inventory differs:" >&2
    diff -u "$tmpdir/native-windows-expected" \
        "$tmpdir/native-windows-actual" >&2 || :
    failed=1
}

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
require_fixed '# if defined(_USE_32BIT_TIME_T)' src/compat_stub.c
require_fixed 'result = _stat32(libc_path,' src/compat_stub.c
require_fixed 'handle = _beginthreadex(NULL, 0,' src/threading.c
# The shell variables are part of the configure.ac text being asserted.
# shellcheck disable=SC2016
require_fixed 'AR="${CONFIG_SHELL-$SHELL} $am_aux_dir/ar-lib lib"' configure.ac
require_fixed 'NM="dumpbin -symbols"' configure.ac
require_fixed 'STRIP=:' configure.ac
require_fixed 'RANLIB=:' configure.ac

require_fixed '#  define SIXEL_PRINTF_ARCHETYPE __MINGW_PRINTF_FORMAT' src/compat_stub.h
require_fixed '_CRTIMP int __cdecl _setmode(int fd, int mode);' src/compat_stub.c
require_fixed '#  define SIXEL_COMPAT_API __declspec(dllexport)' src/compat_stub.h
require_fixed "uuid = cc.find_library('uuid', required: false)" meson.build
require_fixed 'uuid.lib' configure.ac
require_fixed '-luuid' configure.ac

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
require_fixed 'AC_PATH_PROG([RANLIB], [emranlib], [ranlib])' configure.ac
require_fixed 'AC_PATH_PROGS([AR], [emar emer], [ar])' configure.ac
require_fixed "ar = 'emar'" build-aux/meson-cross/emscripten
require_fixed "ranlib = 'emranlib'" build-aux/meson-cross/emscripten
require_fixed 'install-emscripten-sidecar.sh' converters/meson.build
require_fixed 'img2sixel-node-launcher.in' converters/meson.build
require_fixed 'sixel2png-node-launcher.in' converters/meson.build
require_fixed '!defined(_WIN32) && !defined(__EMSCRIPTEN__)' src/tty.c
require_fixed 'HAVE_SYS_SELECT_H && !defined(__EMSCRIPTEN__)' src/tty.c
require_fixed '#if defined(__EMSCRIPTEN__)' src/options.c

require_fixed 'return IsWindows() ? 1 : 0;' src/path.c
require_fixed 'return IsWindows() ? 1 : 0;' converters/path.c

require_fixed "CPPFLAGS=\"\$CPPFLAGS -D_DARWIN_C_SOURCE=1\"" configure.ac
require_fixed "add_project_arguments('-D_DARWIN_C_SOURCE', language: 'c')" meson.build
require_fixed '# define _DARWIN_C_SOURCE' src/threading.c
require_fixed '# undef vsnprintf' src/compat_stub.c
require_fixed 'mib[1] = HW_AVAILCPU;' src/threading.c
require_fixed 'mib[1] = HW_NCPU;' src/threading.c
darwin_define_line=$(awk '/# define _DARWIN_C_SOURCE/ { print NR; exit }' \
    "$src_root/src/threading.c")
darwin_header_line=$(awk '/#  include <sys\/sysctl.h>/ { print NR; exit }' \
    "$src_root/src/threading.c")
test -n "$darwin_define_line" && test -n "$darwin_header_line" &&
    test "$darwin_define_line" -lt "$darwin_header_line" ||
    fail "_DARWIN_C_SOURCE must precede the Darwin sysctl header"

require_fixed '# define _BSD_SOURCE' src/threading.c
require_fixed '# define _NETBSD_SOURCE' src/threading.c
require_fixed '# define _DRAGONFLY_SOURCE' src/threading.c
require_fixed 'fetchIO *fetch_stream = NULL;' src/chunk.c
require_fixed 'fetched = fetchIO_read(fetch_stream, bucket, sizeof(bucket));' src/chunk.c
require_fixed 'HAVE_POSIX_SPAWNP && !defined(__FreeBSD__) && !defined(__DragonFly__)' src/loader-gnome-thumbnailer.c
require_fixed '#if defined(__OpenBSD__)' src/threading.c
require_fixed '#if defined(__FreeBSD__) || defined(__DragonFly__)' \
    src/compat_stub.c
require_fixed '# if defined(__GLIBC__) && defined(_GNU_SOURCE)' \
    src/compat_stub.c
require_fixed 'defined(__ANDROID__))' converters/aborttrace.c

require_fixed '| Debian GNU/Hurd | 2026-03-14 image | amd64 | GCC | Autotools, Meson |' \
    docs/platform-support.md
require_fixed '[GNU/Hurd compatibility](gnu-hurd.md)' \
    docs/misc/platforms/README.md
# The backticks are literal Markdown syntax in the policy text.
# shellcheck disable=SC2016
require_fixed '`x86_64-unknown-gnu0.9`' \
    docs/misc/platforms/gnu-hurd.md
require_fixed '#if defined(__GNU__) && defined(__GNUC__)' \
    src/pthread-once.h
require_fixed '# define SIXEL_PTHREAD_ONCE_DECLARE(name)' \
    src/pthread-once.h
require_fixed '    __extension__ static pthread_once_t name = PTHREAD_ONCE_INIT' \
    src/pthread-once.h
# The make variable is literal source-list syntax.
# shellcheck disable=SC2016
require_fixed '$(srcdir)/pthread-once.h' src/Makefile.am
require_fixed "'pthread-once.h'," src/meson.build
find "$src_root/src" -type f \( -name '*.c' -o -name '*.h' \) \
    ! -name pthread-once.h -exec awk '
    /(^|[^A-Za-z0-9_])PTHREAD_ONCE_INIT([^A-Za-z0-9_]|$)/ {
        print FILENAME ":" FNR ":" $0
    }
' {} + > "$tmpdir/direct-pthread-once-init"
test ! -s "$tmpdir/direct-pthread-once-init" || {
    sed 's/^/# direct PTHREAD_ONCE_INIT use: /' \
        "$tmpdir/direct-pthread-once-init" >&2
    failed=1
}

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
require_fixed 'slice_count=16' .github/actions/ci-steps/action.yml
require_fixed 'for attempt in 1 2 3; do' .github/actions/ci-steps/action.yml
haiku_psd_skip_count=$(grep -lF 'SIXEL_TEST_SKIP_HAIKU_PSD_TYSH_TRACE' \
    "$src_root"/tests/loader/builtin/*.t | awk 'END { print NR }')
test "$haiku_psd_skip_count" -eq 5 ||
    fail "expected 5 narrow Haiku PSD skips, found $haiku_psd_skip_count"
# The shell variable is part of the test text being asserted.
# shellcheck disable=SC2016
require_fixed 'build_os="${RUNTIME_ENV_BUILD_OS-unknown}"' \
    tests/loader/libwebp/0164_loader_libwebp_sigint_pipeline_stop_trace.t
# shellcheck disable=SC2016
require_fixed 'build_os="${RUNTIME_ENV_BUILD_OS-unknown}"' \
    tests/loader/builtin/1249_loader_builtin_gif_sigint_pipeline_stop_trace.t

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
require_fixed 'od -An -tx1 -j16 -N8' \
    tests/cli/sixel2png/0013_size_preserves_aspect_ratio.t
require_fixed '00 00 00 1f 00 00 00 05' \
    tests/cli/sixel2png/0013_size_preserves_aspect_ratio.t
require_fixed 'lsqa_floor=0.98' \
    tests/loader/builtin/0016_lsqa_roundtrip_rgba_small.t
require_fixed '#if defined(_MSC_VER)' assessment/lsqa.c
require_fixed 'errno_t rc;' assessment/lsqa.c

# Solaris awk lacks the GNU third match() argument. Reject its reintroduction
# across project-owned shell, awk, test, workflow, and build-generator code.
find "$src_root/tests" "$src_root/tools" "$src_root/build-aux" \
    "$src_root/.github" -type f \
    \( -name '*.sh' -o -name '*.sh.in' -o -name '*.t' -o -name '*.awk' \
       -o -name '*.yml' -o -name '*.yaml' -o -name '*.am' \) \
    ! -name config.guess ! -name config.sub -exec awk '
function reset_call() {
    in_call=0
    call_depth=0
    call_commas=0
    in_string=0
    in_regex=0
    escaped=0
    expect_operand=0
}
function inspect_fragment(text, file, line_number,
                          remaining, open_at, cursor, character) {
    cursor=1
    while (cursor <= length(text)) {
        if (in_call == 0) {
            remaining=substr(text, cursor)
            if (match(remaining, /(^|[^A-Za-z0-9_])match[[:space:]]*\(/) == 0) {
                return
            }
            open_at=cursor + RSTART + RLENGTH - 2
            in_call=1
            call_depth=1
            call_commas=0
            in_string=0
            in_regex=0
            escaped=0
            expect_operand=1
            call_file=file
            call_line=line_number
            cursor=open_at + 1
            continue
        }
        character=substr(text, cursor, 1)
        if (in_string != 0) {
            if (escaped != 0) {
                escaped=0
            } else if (character == "\\") {
                escaped=1
            } else if (character == "\"") {
                in_string=0
                expect_operand=0
            }
            cursor++
            continue
        }
        if (in_regex != 0) {
            if (escaped != 0) {
                escaped=0
            } else if (character == "\\") {
                escaped=1
            } else if (character == "/") {
                in_regex=0
                expect_operand=0
            }
            cursor++
            continue
        }
        if (character == "\"") {
            in_string=1
        } else if (character == "/" && expect_operand != 0) {
            in_regex=1
        } else if (character == "(") {
            call_depth++
            expect_operand=1
        } else if (character == ")") {
            if (call_depth == 1) {
                if (call_commas >= 2) {
                    print call_file ":" call_line ": match() has three arguments"
                }
                reset_call()
            } else {
                call_depth--
                expect_operand=0
            }
        } else if (character == ",") {
            if (call_depth == 1) {
                call_commas++
            }
            expect_operand=1
        } else if (character !~ /[[:space:]]/) {
            expect_operand=0
        }
        cursor++
    }
}
FILENAME != previous_file {
    reset_call()
    previous_file=FILENAME
}
{
    inspect_fragment($0 "\n", FILENAME, FNR)
}
END {
    if (in_call != 0 && call_commas >= 2) {
        print call_file ":" call_line ": unterminated three-argument match()"
    }
}
' {} + > "$tmpdir/gnu-awk-capture-arrays"
test ! -s "$tmpdir/gnu-awk-capture-arrays" || {
    sed 's/^/# GNU awk capture-array syntax: /' \
        "$tmpdir/gnu-awk-capture-arrays" >&2
    failed=1
}

test "$failed" -eq 0 || {
    echo "not ok 1 - platform compatibility ledger and seams are synchronized"
    exit 1
}

echo "ok 1 - platform compatibility ledger and seams are synchronized"
exit 0
