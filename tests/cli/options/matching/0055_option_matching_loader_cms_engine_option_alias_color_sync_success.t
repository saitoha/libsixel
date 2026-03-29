#!/bin/sh
# TAP test verifying --cms-engine=color-sync alias matches colorsync.

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
output_colorsync="${ARTIFACT_LOCAL_DIR}/cms_engine_alias_colorsync_ref.six"
output_color_sync="${ARTIFACT_LOCAL_DIR}/cms_engine_alias_color_sync_alias.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=colorsync \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_colorsync}" || {
    echo "not ok" 1 - "--cms-engine=colorsync run failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=color-sync \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_color_sync}" || {
    echo "not ok" 1 - "--cms-engine=color-sync alias was rejected"
    exit 0
}

cmp -s "${output_colorsync}" "${output_color_sync}" || {
    echo "not ok" 1 - "--cms-engine=color-sync diverged from --cms-engine=colorsync"
    exit 0
}

echo "ok" 1 - "--cms-engine=color-sync matches --cms-engine=colorsync"
exit 0
