#!/bin/sh
# The metadata test reuses a validated tool instead of searching PATH again.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
# shellcheck disable=SC2016
awk '
function valid(meson, makefile, tap) {
    return index(meson, "test_pkg_config_probe = run_command(sh, \047-c\047, \047\"$1\" --version\047,") &&
        index(meson, "if test_pkg_config_probe.returncode() == 0") &&
        index(meson, "test_pkg_config_path = test_pkg_config.full_path()") &&
        index(meson, "[\047SIXEL_TEST_PKG_CONFIG\047, test_pkg_config_path]") &&
        index(makefile, "SIXEL_TEST_PKG_CONFIG=\047$(PKG_CONFIG)\047; export SIXEL_TEST_PKG_CONFIG;") &&
        index(tap, "test -n \"${SIXEL_TEST_PKG_CONFIG-}\"") &&
        index(tap, "\"${SIXEL_TEST_PKG_CONFIG}\" --static --cflags --libs") &&
        !index(tap, "command -v pkg-config")
}
FILENAME == ARGV[1] && /test_pkg_config|SIXEL_TEST_PKG_CONFIG/ {
    meson = meson $0 "\n"
}
FILENAME == ARGV[2] && /SIXEL_TEST_PKG_CONFIG/ {
    makefile = makefile $0 "\n"
}
FILENAME == ARGV[3] { tap = tap $0 "\n" }
END {
    bare = tap
    gsub(/\$\{SIXEL_TEST_PKG_CONFIG\}/, "pkg-config", bare)
    unchecked = meson
    sub(/returncode\(\) == 0/, "returncode() != 0", unchecked)
    unconfigured = makefile
    gsub(/\$\(PKG_CONFIG\)/, "pkg-config", unconfigured)
    exit !valid(meson, makefile, tap) || valid(meson, makefile, bare) ||
        valid(unchecked, makefile, tap) || valid(meson, unconfigured, tap)
}
' "$1/tests/meson.build" "$1/tests/Makefile.am" \
    "$1/tests/build/pkgconfig/0001_libsixel_pc_resolves.t" || {
    echo 'not ok 1 - metadata test uses configured pkg-config'
    exit 0
}
echo 'ok 1 - metadata test uses configured pkg-config'
exit 0
