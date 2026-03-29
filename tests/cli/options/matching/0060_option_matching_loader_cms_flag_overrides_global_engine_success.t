#!/bin/sh
# TAP test verifying explicit :cms_engine=none overrides global --cms-engine=auto.

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
output_ref_cms0="${ARTIFACT_LOCAL_DIR}/cms_flag_override_ref_cms0.six"
output_with_global_auto_cms0="${ARTIFACT_LOCAL_DIR}/cms_flag_override_global_auto_cms0.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${output_ref_cms0}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=auto \
    -Llibwebp:cms_engine=none! "${input_webp}" >"${output_with_global_auto_cms0}" || {
    echo "not ok" 1 - "decode failed for --cms-engine=auto + cms=0"
    exit 0
}

cmp -s "${output_ref_cms0}" "${output_with_global_auto_cms0}" || {
    echo "not ok" 1 - "cms=0 did not override global --cms-engine=auto"
    exit 0
}
echo "ok" 1 - "cms=0 overrides global --cms-engine=auto"

exit 0
