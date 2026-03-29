#!/bin/sh
# TAP test verifying loader short key :e= maps to cms_engine behavior.

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
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_subkey_ref_cms1.six"
output_ref_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_subkey_ref_cms0.six"
output_subkey_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_subkey_auto.six"
output_subkey_none="${ARTIFACT_LOCAL_DIR}/cms_engine_subkey_none.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "cms=1 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${output_ref_cms0}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:e=auto! "${input_webp}" >"${output_subkey_auto}" || {
    echo "not ok" 1 - "short subkey :e=auto decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:cms_engine=auto:e=none! "${input_webp}" >"${output_subkey_none}" || {
    echo "not ok" 1 - "short subkey :e=none decode failed"
    exit 0
}

cmp -s "${output_ref_cms1}" "${output_subkey_auto}" || {
    echo "not ok" 1 - "short subkey :e=auto did not match cms=1 behavior"
    exit 0
}

cmp -s "${output_ref_cms0}" "${output_subkey_none}" || {
    echo "not ok" 1 - "short subkey :e=none did not match cms=0 behavior"
    exit 0
}

echo "ok" 1 - "short subkey :e=... controls loader cms_engine"
exit 0
