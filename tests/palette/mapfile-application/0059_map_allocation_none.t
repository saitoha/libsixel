#!/bin/sh
# Reject native map work-buffer allocation failure with no diffusion.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_precision/0021_map_allocation_none 2>&1) || {
    echo "not ok 1 - map allocation_none"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map allocation_none"
exit 0
