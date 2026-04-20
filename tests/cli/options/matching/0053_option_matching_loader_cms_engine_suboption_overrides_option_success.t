#!/bin/sh
# TAP test verifying loader cms_engine suboptions override global option.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_subopt_override_ref_cms1.six"
output_ref_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_subopt_override_ref_cms0.six"
output_subopt_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_subopt_override_actual_auto.six"
output_subopt_none="${ARTIFACT_LOCAL_DIR}/cms_engine_subopt_override_actual_none.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "cms=1 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:cms_engine=none! "${input_webp}" >"${output_ref_cms0}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=none \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_subopt_auto}" || {
    echo "not ok" 1 - "cms_engine=auto suboption override decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=auto \
    -Llibwebp:cms_engine=none! "${input_webp}" >"${output_subopt_none}" || {
    echo "not ok" 1 - "cms_engine=none suboption override decode failed"
    exit 0
}

cmp -s "${output_ref_cms1}" "${output_subopt_auto}" || {
    echo "not ok" 1 - "loader cms_engine=auto suboption did not override global option"
    exit 0
}

cmp -s "${output_ref_cms0}" "${output_subopt_none}" || {
    echo "not ok" 1 - "loader cms_engine=none suboption did not override global option"
    exit 0
}

echo "ok" 1 - "loader cms_engine suboptions override global --cms-engine"
exit 0
