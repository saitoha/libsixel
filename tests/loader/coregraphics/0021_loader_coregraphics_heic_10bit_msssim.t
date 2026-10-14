#!/bin/sh
# TAP test: coregraphics decodes 10-bit HEIC content with stable quality.

# Assess the explicitly sized encoder output, after source decoding and
# nearest resampling but before any post-encoding enlargement. The independent
# reference uses the same integer replication; native sample tests stay small.
# Regenerate: python3 tests/data/loader/quality/generate_references.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 192 -h 192 -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heic-10bit-lossless-64.heic" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_heic_10bit_lossless.six" || {
    echo "not ok" 1 - "coregraphics 10-bit HEIC decode failed"
    exit 0
}

lsqa_msg=$(set +x; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/loader/quality/4db0783a-heif-10bit-gradient-reference.ppm" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_heic_10bit_lossless.six" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "coregraphics 10-bit HEIC decode preserves quality"
exit 0
