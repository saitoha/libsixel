#!/bin/sh
# TAP test verifying per-loader CMS engine env overrides global loader default.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"

cms_auto_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -ldisable \
        -Llibwebp:cms_engine=auto! "${input_webp}"
) || {
    echo "not ok" 1 - "libwebp cms=auto reference decode failed"
    exit 0
}

cms_none_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -ldisable \
        -Llibwebp:cms_engine=none! "${input_webp}"
) || {
    echo "not ok" 1 - "libwebp cms=none reference decode failed"
    exit 0
}

test "${cms_auto_output}" != "${cms_none_output}" || {
    echo "not ok" 1 - "libwebp cms references were not distinguishable"
    exit 0
}

global_auto_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -ldisable \
        --env "SIXEL_LOADER_CMS_ENGINE=auto" \
        -Llibwebp! "${input_webp}"
) || {
    echo "not ok" 1 - "global cms engine env (auto) failed"
    exit 0
}

global_none_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -ldisable \
        --env "SIXEL_LOADER_CMS_ENGINE=none" \
        -Llibwebp! "${input_webp}"
) || {
    echo "not ok" 1 - "global cms engine env (none) failed"
    exit 0
}

override_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -ldisable \
        --env "SIXEL_LOADER_CMS_ENGINE=none" \
        --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=auto" \
        -Llibwebp! "${input_webp}"
) || {
    echo "not ok" 1 - "per-loader cms engine override failed"
    exit 0
}

test "${global_auto_output}" = "${cms_auto_output}" || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=auto did not match reference"
    exit 0
}

test "${global_none_output}" = "${cms_none_output}" || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=none did not match reference"
    exit 0
}

test "${override_output}" = "${cms_auto_output}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE did not override global"
    exit 0
}

test "${override_output}" != "${global_none_output}" || {
    echo "not ok" 1 - "override output matched global none baseline"
    exit 0
}

echo "ok" 1 - "per-loader cms engine env overrides global cms engine env"
exit 0
