#!/bin/sh
# TAP test: quicklook decodes multi-size ICO with stable visual quality.

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

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 192 -h 192 -L quicklook:thumbnail_size=64! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_ico_multisize.six" || {
    echo "not ok" 1 - "quicklook ICO multisize decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/loader/quality/137ef5f9-snake-64-reference-rgba.ppm" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_ico_multisize.six" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "quicklook ICO multisize decode preserves quality"
exit 0
