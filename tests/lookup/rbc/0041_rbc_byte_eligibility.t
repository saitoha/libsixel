#!/bin/sh
# Verify rbc byte eligibility.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/lookup_float/0001_lookup_palette byte 6 0 0 2>&1) || {
    printf '\n%s\n' "not ok 1 - rbc byte eligibility"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/rbc.8bit"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual byte lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - rbc byte eligibility"
exit 0
