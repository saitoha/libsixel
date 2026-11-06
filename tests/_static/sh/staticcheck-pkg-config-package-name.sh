#!/bin/sh
# Exercise the metadata test with a package-name-only pkg-config tool.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-package-name-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
: > "$tmpdir/libsixel.pc"
# A shell function models the selected tool without building or executing a
# temporary program. The subprocess inherits it by sourcing the real test.
# shellcheck disable=SC2329
package_probe() {
    test "$*" = '--static --cflags --libs libsixel' || return 1
    test "${PKG_CONFIG_PATH%%:*}" = . || return 1
    test -f libsixel.pc || return 1
    printf '%s\n' '-lsixel'
}
result=$(TOP_BUILDDIR="$tmpdir"
    SIXEL_TEST_PKG_CONFIG=package_probe
    # shellcheck disable=SC1091
    . "$src_root/tests/build/pkgconfig/0001_libsixel_pc_resolves.t")
test "$result" = '1..1
ok 1 - pkg-config resolves libsixel.pc with its private requirements' || {
    echo 'not ok 1 - metadata resolution depends on filename support'
    exit 0
}
echo 'ok 1 - metadata resolution uses a package name and local search path'
exit 0
