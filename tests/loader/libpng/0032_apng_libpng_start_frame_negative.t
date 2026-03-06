#!/bin/sh
# TAP test: libpng APNG start frame accepts negative indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -v -Llibpng! -S \
              "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
              >"${ARTIFACT_LOCAL_DIR}/apng_start_default_neg.six" || {
    echo "not ok" 1 - "baseline APNG decode failed"
    exit 0
}

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -v -Llibpng! -S \
              --start-frame=-1 \
              "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
              >"${ARTIFACT_LOCAL_DIR}/apng_start_negative.six" || {
    echo "not ok" 1 - "APNG decode with negative start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/apng_start_default_neg.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_start_negative.six" && {
    echo "not ok" 1 - "negative start frame did not change static APNG output"
    exit 0
}

echo "ok" 1 - "libpng APNG negative start frame is applied"
exit 0
