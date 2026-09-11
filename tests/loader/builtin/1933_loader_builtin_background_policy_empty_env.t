#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify an empty background-policy environment value uses the default.
# Policy: docs/loader/background-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png"

default_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite -Lbuiltin:cms_engine=none! -B#fff \
    -d fs:scan=raster -o - "${input_png}") || {
    echo "not ok 1 - default background-policy render failed"
    exit 0
}

empty_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_BACKGROUND_POLICY= \
    -A composite -Lbuiltin:cms_engine=none! -B#fff \
    -d fs:scan=raster -o - "${input_png}") || {
    echo "not ok 1 - empty background-policy render failed"
    exit 0
}

test "${empty_output}" = "${default_output}" || {
    echo "not ok 1 - empty background policy did not use file_first"
    exit 0
}

echo "ok 1 - empty background policy uses the file_first default"
exit 0
