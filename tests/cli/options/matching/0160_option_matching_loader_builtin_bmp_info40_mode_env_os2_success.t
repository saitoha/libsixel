#!/bin/sh
# TAP test verifying SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE=os2 is applied.

set -eux


test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_bmp="${TOP_SRCDIR}/tests/data/inputs/formats/bmp-info40-os2-huffman1d-2x2.bmp"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE=windows" \
    -Lbuiltin! "${input_bmp}" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "windows env unexpectedly succeeded on OS/2 info40 sample"
    exit 0
}

test "${msg#*invalid argument for -L,--loaders option*}" = "${msg}" || {
    echo "not ok" 1 - "windows env path failed at option parsing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE=os2" \
    -Lbuiltin! "${input_bmp}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "os2 env did not enable OS/2 info40 decode"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE=os2 is applied"
exit 0
