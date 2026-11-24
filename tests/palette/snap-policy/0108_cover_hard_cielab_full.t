#!/bin/sh
# Verify native snap and cover: cover hard cielab full.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract cover 0 3 1 3 2>&1) || {
    printf '\n%s\n' "not ok 1 - cover hard cielab full"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - cover hard cielab full"
exit 0
