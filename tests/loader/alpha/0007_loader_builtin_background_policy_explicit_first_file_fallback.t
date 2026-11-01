#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify explicit_first falls back to a PNG file background.
# Policy: docs/loader/background-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgyn6a16.png"

file_fallback=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -o - "${input_png}") || {
    echo "not ok 1 - explicit_first PNG fallback render failed"
    exit 0
}

explicit_yellow=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first -B '#ffff00' \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -o - "${input_png}") || {
    echo "not ok 1 - explicit yellow PNG render failed"
    exit 0
}

explicit_black=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first -B '#000000' \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -o - "${input_png}") || {
    echo "not ok 1 - explicit black PNG render failed"
    exit 0
}

test "${file_fallback}" = "${explicit_yellow}" || {
    echo "not ok 1 - PNG bKGD fallback did not select yellow"
    exit 0
}

test "${file_fallback}" != "${explicit_black}" || {
    echo "not ok 1 - PNG bKGD fallback matched the black control"
    exit 0
}

echo "ok 1 - explicit_first falls back to the PNG file background"
exit 0
