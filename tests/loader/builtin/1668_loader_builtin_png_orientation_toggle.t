#!/bin/sh
# TAP test confirming builtin orientation toggle affects PNG eXIf decode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.png"
output_on="${ARTIFACT_ROOT}/${0##*/}.on.png"
output_off="${ARTIFACT_ROOT}/${0##*/}.off.png"
dims_on=''
dims_off=''

SIXEL_LOADER_BUILTIN_ORIENTATION=on
export SIXEL_LOADER_BUILTIN_ORIENTATION
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o "${output_on}" "${input_png}" >/dev/null || {
    echo "not ok" 1 - "builtin PNG orientation on decode failed"
    exit 0
}

SIXEL_LOADER_BUILTIN_ORIENTATION=off
export SIXEL_LOADER_BUILTIN_ORIENTATION
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o "${output_off}" "${input_png}" >/dev/null || {
    echo "not ok" 1 - "builtin PNG orientation off decode failed"
    exit 0
}

dims_on=$(od -tx1 -j16 -N8 "${output_on}" | awk '
    NR == 1 {
        i = NF - 7
        while (i <= NF) {
            printf "%s", $i
            i = i + 1
        }
        exit
    }')
dims_off=$(od -tx1 -j16 -N8 "${output_off}" | awk '
    NR == 1 {
        i = NF - 7
        while (i <= NF) {
            printf "%s", $i
            i = i + 1
        }
        exit
    }')

test "${dims_on}" = "000000080000000c" || {
    echo "not ok" 1 - "builtin PNG orientation on expected 8x12 PNG IHDR"
    exit 0
}
test "${dims_off}" = "0000000c00000008" || {
    echo "not ok" 1 - "builtin PNG orientation off expected 12x8 PNG IHDR"
    exit 0
}

echo "ok" 1 - "builtin PNG orientation toggle changes output geometry"
exit 0
