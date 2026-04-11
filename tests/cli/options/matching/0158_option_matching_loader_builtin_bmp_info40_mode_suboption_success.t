#!/bin/sh
# TAP test verifying builtin bmp_info40_mode suboption accepts auto.

set -eux


test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_bmp="${TOP_SRCDIR}/tests/data/inputs/formats/bmp-info40-os2-huffman1d-2x2.bmp"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:bmp_info40_mode=auto! \
    "${input_bmp}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "builtin:bmp_info40_mode=auto suboption decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*invalid argument for -L,--loaders option*}" = "${msg}" || {
    echo "not ok" 1 - "builtin:bmp_info40_mode=auto was rejected by option parser"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "-L accepts builtin:bmp_info40_mode=auto"
exit 0
