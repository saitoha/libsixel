#!/bin/sh
# TAP test verifying libtiff cms_engine suboption overrides per-loader env.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_tiff="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.tiff"
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_libtiff_subopt_ref_cms1.six"
output_ref_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_libtiff_subopt_ref_cms0.six"
output_subopt_none="${ARTIFACT_LOCAL_DIR}/cms_engine_libtiff_subopt_none.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=auto! "${input_tiff}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "libtiff cms=1 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=none! "${input_tiff}" >"${output_ref_cms0}" || {
    echo "not ok" 1 - "libtiff cms=0 reference decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_ref_cms1}" "${output_ref_cms0}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "libtiff cms references were not distinguishable: ${lsqa_msg-}"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBTIFF_CMS_ENGINE=auto" \
    -Llibtiff:cms_engine=none! "${input_tiff}" >"${output_subopt_none}" || {
    echo "not ok" 1 - "libtiff cms_engine=none suboption decode failed"
    exit 0
}

cmp -s "${output_ref_cms0}" "${output_subopt_none}" || {
    echo "not ok" 1 - "libtiff cms_engine=none suboption did not override per-loader env"
    exit 0
}

echo "ok" 1 - "libtiff cms_engine suboption overrides per-loader env"
exit 0
