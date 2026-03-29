#!/bin/sh
# TAP test verifying explicit :cms_engine=auto overrides global --cms-engine=none.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64_embedded_a98_icc.webp"
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_flag_override_ref_cms1.six"
output_with_global_none_cms1="${ARTIFACT_LOCAL_DIR}/cms_flag_override_global_none_cms1.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "cms=1 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=none \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_with_global_none_cms1}" || {
    echo "not ok" 1 - "decode failed for --cms-engine=none + cms=1"
    exit 0
}

cmp -s "${output_ref_cms1}" "${output_with_global_none_cms1}" || {
    echo "not ok" 1 - "cms=1 did not override global --cms-engine=none"
    exit 0
}
echo "ok" 1 - "cms=1 overrides global --cms-engine=none"

exit 0
