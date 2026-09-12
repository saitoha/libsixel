#!/bin/sh
# Verify default auto alpha policy selects keep for 6delta output.
# Policy: docs/functionality/delta-encoding.md
# Policy: docs/loader/alpha-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"
esc="$(printf '\033')"

output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --update-policy=delta:threshold=0 -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "default 6delta alpha-policy render failed"
    exit 0
}

test "${output#"${esc}P0;1q"}" != "${output}" || {
    echo "not ok" 1 - "6delta did not resolve auto alpha-policy to keep"
    exit 0
}

echo "ok" 1 - "6delta resolves default auto alpha-policy to keep"
exit 0
