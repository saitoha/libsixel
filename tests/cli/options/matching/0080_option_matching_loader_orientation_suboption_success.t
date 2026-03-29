#!/bin/sh
# TAP test verifying libpng orientation suboption is accepted by -L.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

probe_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=on! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.png" \
    -o/dev/null 2>&1) || probe_status=$?

test "${probe_output#*invalid argument for -L,--loaders option*}" = \
    "${probe_output}" || {
    echo "not ok" 1 - "-L libpng orientation suboption was rejected"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${probe_output}" >&2
    exit 0
}

test "${probe_status-}" = "" || {
    echo "not ok" 1 - "libpng orientation suboption decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${probe_output}" >&2
    exit 0
}

echo "ok" 1 - "-L accepts libpng:orientation suboption"
exit 0
