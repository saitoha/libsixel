#!/bin/sh
# Verify mahalanobis gamma p3.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/mahalanobis_float/0002_mah_gamma_p3 2>&1) || {
    echo "not ok 1 - mahalanobis gamma p3"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/mahalanobis.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual float lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - mahalanobis gamma p3"
exit 0
