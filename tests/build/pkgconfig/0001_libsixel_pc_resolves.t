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

# A direct .pc pathname also works when a native Windows drive letter would
# be mistaken for a separator by the MSYS pkg-config search-path parser.
resolved=$(set +xv; pkg-config --static --cflags --libs "$pc_file" 2>&1) || {
    echo "not ok 1 - pkg-config cannot resolve libsixel.pc"
    printf '%s\n' "${resolved}" >&2
    exit 0
}

echo "ok 1 - pkg-config resolves libsixel.pc with its private requirements"
exit 0
