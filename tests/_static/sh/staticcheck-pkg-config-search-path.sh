#!/bin/sh
# Prepending the build directory preserves POSIX and Windows search lists.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-package-path-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
: > "$tmpdir/libsixel.pc"
# Model each tool's path grammar without requiring a native Windows binary.
# shellcheck disable=SC2329
path_probe() {
    test "$PKG_CONFIG_PATH" = "$expected_path" || return 1
    test -f libsixel.pc || return 1
    printf '%s\n' '-lsixel'
}
for expected_path in . '.:/opt/pkg/lib/pkgconfig' \
    '.:/opt/pkg/lib/pkgconfig:/opt/extra/share/pkgconfig' \
    '.;D:\package files\lib\pkgconfig' \
    '.;D:\packages\lib\pkgconfig;D:\packages\share\pkgconfig' \
    '.;D:/package files/lib/pkgconfig' \
    '.;D:/packages/lib/pkgconfig;D:/packages/share/pkgconfig' \
    '.;relative/lib;relative/share'; do
    result=$(TOP_BUILDDIR="$tmpdir"
        SIXEL_TEST_PKG_CONFIG=path_probe
        PKG_CONFIG_PATH=${expected_path#.}
        PKG_CONFIG_PATH=${PKG_CONFIG_PATH#?}
        # shellcheck disable=SC1091
        . "$src_root/tests/build/pkgconfig/0001_libsixel_pc_resolves.t")
    test "$result" = '1..1
ok 1 - pkg-config resolves libsixel.pc with its private requirements' || {
        echo 'not ok 1 - metadata search path changed its separator or entries'
        exit 0
    }
done
echo 'ok 1 - metadata search paths preserve POSIX and Windows list syntax'
exit 0
