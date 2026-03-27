#!/bin/sh
# TAP test: builtin APNG blend-over static rendering matches reference.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_blend_over.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_blend_over_builtin_static.six" || {
    echo "not ok" 1 - "builtin APNG blend-over static rendering failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_blend_over_builtin_static_reference.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_blend_over_builtin_static.six" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin APNG blend-over static rendering matches reference"
exit 0
