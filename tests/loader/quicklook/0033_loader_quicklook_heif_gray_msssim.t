#!/bin/sh
# TAP test: quicklook decodes grayscale HEIF with stable visual quality.

# Assess the explicitly sized encoder output, after source decoding and
# nearest resampling but before any post-encoding enlargement. The independent
# reference uses the same integer replication; native sample tests stay small.
# Regenerate: python3 tests/data/loader/quality/generate_references.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

test "${SIXEL_TEST_HOST_ARCH-}" != "x86_64" || {
    printf "1..0 # SKIP quicklook coverage is unstable on x86_64 for this input\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 192 -h 192 -L quicklook:thumbnail_size=64! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heif-gray-lossless-64.heif" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_heif_gray.six" || {
    echo "not ok" 1 - "quicklook HEIF grayscale decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/loader/quality/ecb29f68-snake-64-reference-gray.ppm" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_heif_gray.six" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "quicklook HEIF grayscale decode preserves quality"
exit 0
