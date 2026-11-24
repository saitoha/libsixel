#!/bin/sh
# Verify eytzinger gamma p3.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/eytzinger_float/0002_eyt_gamma_p3 2>&1) || {
    printf '\n%s\n' "not ok 1 - eytzinger gamma p3"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/eytzinger.float32"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual float lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - eytzinger gamma p3"
exit 0
