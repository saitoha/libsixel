#!/bin/sh
# Verify -B overrides a conflicting SIXEL_BGCOLOR value.
# Policy: docs/loader/background-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"

cli_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first \
    --env SIXEL_BGCOLOR=#000000 -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok 1 - CLI background precedence render failed"
    exit 0
}

white_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first \
    --env SIXEL_BGCOLOR=#ffffff \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok 1 - white background control render failed"
    exit 0
}

black_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite --background-policy=explicit_first \
    --env SIXEL_BGCOLOR=#000000 \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok 1 - black background control render failed"
    exit 0
}

test "${cli_output}" = "${white_output}" || {
    echo "not ok 1 - -B did not override SIXEL_BGCOLOR"
    exit 0
}

test "${cli_output}" != "${black_output}" || {
    echo "not ok 1 - background controls did not produce distinct colors"
    exit 0
}

echo "ok 1 - -B overrides SIXEL_BGCOLOR"
exit 0
