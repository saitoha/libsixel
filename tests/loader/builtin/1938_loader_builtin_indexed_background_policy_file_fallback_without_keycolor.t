#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify indexed PNG bKGD fallback when key-color mode is disabled.
# Policy: docs/loader/background-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbyn3p08.png"

file_fallback=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first \
    -Lbuiltin:cms_engine=none:trns_keycolor=0! -d fs:scan=raster \
    -o - "${input_png}") || {
    echo "not ok 1 - indexed PNG bKGD fallback render failed"
    exit 0
}

explicit_yellow=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first -B '#ffff00' \
    -Lbuiltin:cms_engine=none:trns_keycolor=0! -d fs:scan=raster \
    -o - "${input_png}") || {
    echo "not ok 1 - indexed PNG explicit yellow render failed"
    exit 0
}

explicit_black=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first -B '#000000' \
    -Lbuiltin:cms_engine=none:trns_keycolor=0! -d fs:scan=raster \
    -o - "${input_png}") || {
    echo "not ok 1 - indexed PNG explicit black render failed"
    exit 0
}

test "${file_fallback}" = "${explicit_yellow}" || {
    echo "not ok 1 - indexed PNG bKGD did not select yellow"
    exit 0
}

test "${file_fallback}" != "${explicit_black}" || {
    echo "not ok 1 - indexed PNG bKGD matched the black control"
    exit 0
}

echo "ok 1 - indexed PNG bKGD fallback works without key-color mode"
exit 0
