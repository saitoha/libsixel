#!/bin/sh
# Verify mahalanobis gamma srgb.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/mahalanobis_float/0001_mah_gamma_srgb 2>&1) || {
    printf '\n%s\n' "not ok 1 - mahalanobis gamma srgb"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/mahalanobis.float32"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual float lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - mahalanobis gamma srgb"
exit 0
