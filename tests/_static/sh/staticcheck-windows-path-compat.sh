#!/bin/sh
# Verify Windows cross-runtime CI and path conversion boundaries.
# Policy: docs/misc/platforms/windows-paths.md

set -eu

src_root=$1
failed=0

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

require_path_copy()
{
    pattern=$1

    require_fixed "$pattern" src/path.c
    require_fixed "$pattern" converters/path.c
}

echo "1..1"

# Active jobs are evidence for distinct driver-shell and target-runtime pairs.
require_fixed 'label: Meson-cmd-x86_64-msvc-x86_64' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-msys2-msys-msvc' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-msys2-msys-vs_clangcl' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-cygwin64-msvc' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-cygwin64-clangcl' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-cygwin64-clang_msvc_abi' \
    .github/workflows/ci.yml
require_fixed 'CC="clang --driver-mode=cl"' .github/workflows/ci.yml
require_fixed '--host=x86_64-pc-windows-msvc' .github/workflows/ci.yml
require_fixed 'label: Autotools-msys2-clang64' .github/workflows/ci.yml
require_fixed 'label: Meson-msys2-clang64' .github/workflows/ci.yml
require_fixed 'mingw-w64-clang-x86_64-toolchain' .github/workflows/ci.yml
require_fixed 'label: Autotools-msys2-mingw32' .github/workflows/ci.yml
require_fixed 'label: Autotools-msys2-mingw64-static-coverage' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-msys2-ucrt64-coverage' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-msys2-msys-gcc-coverage' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-cygwin64-gcc' .github/workflows/ci.yml
require_fixed 'label: Meson-cygwin64-gcc-unity' .github/workflows/ci.yml
require_fixed 'label: Autotools-ubuntu-x86_64-mingw-w64-x86_64-win32-wine64-static' \
    .github/workflows/ci.yml
require_fixed 'SIXEL_RUNTIME=wine' .github/workflows/ci.yml

# Windows-hosted Emscripten remains an explicit gap until a public row exists.
windows_emscripten=$(awk '
function inspect() {
    if (label ~ /emscripten/ && runner ~ /^windows/) {
        print label " -> " runner
    }
}
/^          - label: / {
    inspect()
    label=$0
    sub(/^          - label: /, "", label)
    runner=""
    next
}
/^            runner: / {
    runner=$0
    sub(/^            runner: /, "", runner)
}
END { inspect() }
' "$src_root/.github/workflows/ci.yml")
test -z "$windows_emscripten" ||
    fail "Windows-hosted Emscripten row requires policy update: $windows_emscripten"
require_fixed 'label: Autotools-ubuntu-aarch64-emscripten-static' \
    .github/workflows/ci.yml
require_fixed 'label: Autotools-macos-aarch64-emscripten-static' \
    .github/workflows/ci.yml

# The MSYS2/Meson MSVC adapter is dormant until an active row adopts it.
meson_msys_msvc=$(awk '
/^          - label: Meson-msys2/ && /msvc/ {
    label=$0
    sub(/^          - label: /, "", label)
    print label
}
' "$src_root/.github/workflows/ci.yml")
test -z "$meson_msys_msvc" ||
    fail "Meson/MSYS2/MSVC row requires matrix policy update: $meson_msys_msvc"

# MSYS2 must distinguish path-bearing argv from MSVC slash options.
# shellcheck disable=SC2016
require_fixed 'MSVC_BIN_PATH="$(cygpath -u -p "$MSVC_PATH")"' \
    .github/actions/ci-steps/action.yml
require_fixed '*/Microsoft\ Visual\ Studio/*/VC/Tools/MSVC/*/bin/*)' \
    .github/actions/ci-steps/action.yml
require_fixed '@/[A-Za-z]/*)' .github/actions/ci-steps/action.yml
require_fixed '/*:/[A-Za-z]/*)' .github/actions/ci-steps/action.yml
require_fixed '/Fe/[A-Za-z]/*|/Fo/[A-Za-z]/*|/Fd/[A-Za-z]/*|/Fp/[A-Za-z]/*|/Fa/[A-Za-z]/*)' \
    .github/actions/ci-steps/action.yml
require_fixed '/I/[A-Za-z]/*)' .github/actions/ci-steps/action.yml
require_fixed '-I/[A-Za-z]/*|-L/[A-Za-z]/*)' \
    .github/actions/ci-steps/action.yml
require_fixed "export MSYS2_ARG_CONV_EXCL='*'" \
    .github/actions/ci-steps/action.yml
# shellcheck disable=SC2016
require_fixed 'exec "$real_tool" "${converted[@]}"' \
    .github/actions/ci-steps/action.yml

# Test launch keeps executable selection, DLL lookup, and runtime prefix apart.
require_fixed 'AC_ARG_VAR([SIXEL_TEST_ADDITIOANL_PATH]' configure.ac
require_fixed 'AC_ARG_VAR([SIXEL_TEST_ADDITIONAL_PATH]' configure.ac
require_fixed "SIXEL_LIBTOOL_PATH_SEPARATOR=':'" configure.ac
require_fixed 'shlibpath_overrides_runpath=*' \
    build-aux/resolve-test-tool-paths.sh.in
require_fixed '@SIXEL_LIBTOOL_OBJDIR@/img2sixel@EXEEXT@' \
    build-aux/resolve-test-tool-paths.sh.in
require_fixed 'quote_single()' build-aux/resolve-test-tool-paths.sh.in
require_fixed "bin_ext = '.exe'" tests/meson.build
require_fixed "test_env.prepend('PATH', dll_path)" tests/meson.build
require_fixed "export SIXEL_RUNTIME=''" tests/Makefile.am
# shellcheck disable=SC2016
require_fixed '$(SIXEL_RUNTIME) "$$test_runner_probe"' tests/Makefile.am

# Library and standalone-converter copies must retain equivalent semantics.
require_path_copy 'parse_nested_cygdrive(char const *path,'
require_path_copy 'cygwin_conv_path(CCP_WIN_A_TO_POSIX, path, NULL, 0)'
require_path_copy 'static char const *prefix = "cygpath -wa -- ";'
require_path_copy 'GetProcAddress(ntdll, "wine_get_version")'
require_path_copy 'static char const *clipboard_prefix = "clipboard:";'
require_path_copy 'if (strcmp(path, "-") == 0) {'
require_path_copy 'return path[0] == '\''/'\'' || path[0] == '\''~'\'';'
require_fixed 'sixel_path_to_libc_buffer_size(char const *path)' src/path.c
require_fixed 'sixel_path_to_libc(char const *path,' src/path.c
require_fixed 'img2sixel_path_to_libc_buffer_size(char const *path)' \
    converters/path.c
require_fixed 'img2sixel_path_to_libc(char const *path,' converters/path.c
require_fixed 'The helper never allocates.' src/path.h
require_fixed 'The helper never allocates.' converters/path.h
require_fixed 'buffer_size = sixel_path_to_libc_buffer_size(path);' \
    src/compat_stub.c
require_fixed '#if defined(SIXEL_AMALGAMATION)' converters/compat.c
require_fixed 'buffer_size = sixel_path_to_libc_buffer_size(path);' \
    converters/compat.c
require_fixed 'buffer_size = img2sixel_path_to_libc_buffer_size(path);' \
    converters/compat.c

# CreateProcessA needs its own command-token normalization and quoting boundary.
require_fixed 'test_runner_duplicate_win32_path(char const *path)' \
    tests/test_runner.c
require_fixed 'strncmp(path, "/cygdrive/", 10u) == 0' tests/test_runner.c
require_fixed 'create_ok = CreateProcessA(program,' tests/test_runner.c
require_fixed 'GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,' tests/test_runner.c

test "$failed" -eq 0 || {
    echo "not ok 1 - Windows cross-runtime path contracts are synchronized"
    exit 1
}

echo "ok 1 - Windows cross-runtime path contracts are synchronized"
exit 0
