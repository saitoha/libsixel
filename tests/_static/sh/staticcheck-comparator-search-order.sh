#!/bin/sh
# Project comparators must precede tools belonging to another shell runtime.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
# shellcheck disable=SC2016
awk '
function valid(driver, meson, suite) {
    return index(driver, "PATH=\"${TOP_SRCDIR}/build-aux/compare:$PATH\"") &&
        index(driver, "PATH=\"${TOP_BUILDDIR}/tools/compare:$PATH\"") &&
        index(driver, "build-aux/compare:$PATH") < index(driver, "tools/compare:$PATH") &&
        index(meson, "env.prepend(\047PATH\047, meson.project_source_root() / \047build-aux/compare\047)") &&
        index(meson, "env.prepend(\047PATH\047, meson.project_build_root() / \047tools/compare\047)") &&
        index(meson, "build-aux/compare") < index(meson, "tools/compare") &&
        index(suite, "PATH=\"$build_root/tools/compare:$src_root/build-aux/compare:$PATH\"")
}
FILENAME == ARGV[1] && /^ *PATH=/ { driver = driver $0 "\n" }
FILENAME == ARGV[2] && /env\.(prepend|append)\(/ {
    meson = meson $0 "\n"
}
FILENAME == ARGV[3] && /^PATH=/ { suite = suite $0 "\n" }
END {
    appended = driver
    gsub(/compare:\$PATH/, "compare", appended)
    ambient = meson
    gsub(/prepend/, "append", ambient)
    exit !valid(driver, meson, suite) || valid(appended, meson, suite) ||
        valid(driver, ambient, suite) || valid(driver, meson, "")
}' "$1/build-aux/lso-tap-driver.sh.in" "$1/tests/meson.build" \
    "$1/tests/_static/sh/staticcheck-suite.sh" || {
    echo 'not ok 1 - ambient comparison tool can shadow the owned helper'
    exit 0
}
echo 'ok 1 - project comparison tools precede ambient utilities'
exit 0
