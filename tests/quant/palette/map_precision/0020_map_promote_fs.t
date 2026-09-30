#!/bin/sh
# Promote byte samples for native FS and verify nonzero feedback.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_precision/0020_map_promote_fs 2>&1) || {
    echo "not ok 1 - map promote_fs"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/none.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual float32 consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map promote_fs"
exit 0
