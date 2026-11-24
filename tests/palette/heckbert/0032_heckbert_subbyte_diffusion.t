#!/bin/sh
# Sub-byte source distinctions keep requested diffusion active.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

# dither_contract retains the state checked by C; direct frame input does not
# emit the outer file-encoder trace. Observe its actual lookup separately.
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0032_heckbert_subbyte_diffusion 2>&1) || {
    printf '\n%s\n' "not ok 1 - native source color count"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/none.float32"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual float lookup consumer was bypassed"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - native distinctions keep FS active"
exit 0
