#!/bin/sh
# TAP test confirming builtin orientation toggle affects JPEG EXIF decode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.jpg"
output_on="${ARTIFACT_ROOT}/${0##*/}.on.png"
output_off="${ARTIFACT_ROOT}/${0##*/}.off.png"
dims_on=''
dims_off=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_BUILTIN_ORIENTATION=on \
    -L builtin! -o "${output_on}" "${input_jpeg}" >/dev/null || {
    echo "not ok" 1 - "builtin JPEG orientation on decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_BUILTIN_ORIENTATION=off \
    -L builtin! -o "${output_off}" "${input_jpeg}" >/dev/null || {
    echo "not ok" 1 - "builtin JPEG orientation off decode failed"
    exit 0
}

dims_on=$(od -An -tx1 -j16 -N8 "${output_on}" | tr -d ' \n')
dims_off=$(od -An -tx1 -j16 -N8 "${output_off}" | tr -d ' \n')

test "${dims_on}" = "000000080000000c" || {
    echo "not ok" 1 - "builtin JPEG orientation on expected 8x12 PNG IHDR"
    exit 0
}
test "${dims_off}" = "0000000c00000008" || {
    echo "not ok" 1 - "builtin JPEG orientation off expected 12x8 PNG IHDR"
    exit 0
}

echo "ok" 1 - "builtin JPEG orientation toggle changes output geometry"
exit 0
