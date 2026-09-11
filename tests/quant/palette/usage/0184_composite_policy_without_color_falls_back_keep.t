#!/bin/sh
# Verify composite policy falls back to P2=1 without a resolved color.
# Policy: docs/loader/alpha-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Preserve the unmanaged fixture values; CMS defaults are tested separately.
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"
esc="$(printf '\033')"
expected="${esc}P0;1q\"1;1;1;1#0;2;1;1;1${esc}\\"

composite_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
    --alpha-policy=composite \
    -L builtin:osc11_query=0! -d fs:scan=raster -o - \
    "${input_image}") || {
    echo "not ok" 1 - "composite alpha-policy render failed"
    exit 0
}
test "${composite_output}" = "${expected}" || {
    echo "not ok" 1 - "composite fallback painted the alpha-zero pixel"
    exit 0
}

echo "ok" 1 - "composite fallback omits alpha zero and emits P2=1"
exit 0
