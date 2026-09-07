#!/bin/sh
# Verify -A takes precedence over SIXEL_ALPHA_POLICY regardless of order.
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

env_first=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_ALPHA_POLICY=composite --alpha-policy=clear \
    -B '#ffffff' -L builtin! -d fs:scan=raster -o - \
    "${input_image}") || {
    echo "not ok" 1 - "alpha-policy environment-first render failed"
    exit 0
}

cli_first=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=clear --env SIXEL_ALPHA_POLICY=composite \
    -B '#ffffff' -L builtin! -d fs:scan=raster -o - \
    "${input_image}") || {
    echo "not ok" 1 - "alpha-policy CLI-first render failed"
    exit 0
}

test "${env_first}" = "${cli_first}" || {
    echo "not ok" 1 - "alpha-policy precedence depends on option order"
    exit 0
}

test "${env_first#"${esc}P0;0q"}" != "${env_first}" || {
    echo "not ok" 1 - "alpha-policy CLI value did not override environment"
    exit 0
}

echo "ok" 1 - "alpha-policy CLI value overrides environment"
exit 0
