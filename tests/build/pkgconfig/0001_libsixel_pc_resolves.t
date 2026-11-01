#!/bin/sh
# Resolve the generated libsixel.pc, including Requires.private, through
# pkg-config so every private requirement names an installed package.

set -eux

pc_file="${TOP_BUILDDIR}/libsixel.pc"

command -v pkg-config >/dev/null 2>&1 || {
    echo "1..0 # SKIP pkg-config is unavailable"
    exit 0
}
test -f "${pc_file}" || {
    echo "1..0 # SKIP libsixel.pc is not generated in this build"
    exit 0
}

echo "1..1"
set -v

resolved=$(set +xv; PKG_CONFIG_PATH="${TOP_BUILDDIR}${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}" \
    pkg-config --static --cflags --libs libsixel 2>&1) || {
    echo "not ok 1 - pkg-config cannot resolve libsixel.pc"
    printf '%s\n' "${resolved}" >&2
    exit 0
}

echo "ok 1 - pkg-config resolves libsixel.pc with its private requirements"
exit 0
