#!/bin/sh
# TAP test confirming builtin HDR tonemap output avoids black collapse.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_midtones.hdr"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin-hdr-tonemap-reinhard.six"
black_reference="${ARTIFACT_LOCAL_DIR}/builtin-hdr-black-reference.ppm"
lsqa_status=0

SIXEL_LOADER_HDR_TONEMAP=reinhard \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! "${input_hdr}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode HDR with tonemap=reinhard"
    exit 0
}

printf 'P6\n1 1\n255\n\000\000\000' >"${black_reference}"
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.99" \
    "${black_reference}" "${output_sixel}" 2>&1) || lsqa_status=$?
lsqa_status=${lsqa_status-0}

test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "builtin HDR tonemap output collapsed to black-like image: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin loader decodes HDR with tonemap=reinhard and avoids black output collapse"
exit 0
