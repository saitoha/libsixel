#!/bin/sh
# TAP test verifying global env alias color-sync maps to colorsync.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64_embedded_a98_icc.webp"
ref_none_cksum=''
env_colorsync_cksum=''
env_color_sync_cksum=''

set +x
ref_none_cksum=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:cms_engine=none! "${input_webp}" | cksum) || {
    set -x
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

env_colorsync_cksum=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_ENGINE=colorsync" \
    -Llibwebp! "${input_webp}" | cksum) || {
    set -x
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=colorsync decode failed"
    exit 0
}

env_color_sync_cksum=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_ENGINE=color-sync" \
    -Llibwebp! "${input_webp}" | cksum) || {
    set -x
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=color-sync decode failed"
    exit 0
}
set -x

test "${env_colorsync_cksum}" != "${ref_none_cksum}" || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=colorsync did not differ from none baseline"
    exit 0
}

test "${env_colorsync_cksum}" = "${env_color_sync_cksum}" || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=color-sync alias did not match colorsync behavior"
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_CMS_ENGINE=color-sync alias maps to colorsync"
exit 0
