#!/bin/sh
# TAP test verifying -Q accepts long-form center suboptions.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:algo=hybrid:profile=balance:auto_policy=adaptive:auto_fft_threshold=4096:space_policy=perceptual:candidate_policy=hybrid:seed=42:restarts=2:init_seeds=2:iter=8:histbits=5:point_budget=512:rare_keep=64:prune_mass=0.980:budget_policy=adaptive:budget_scale=1.25:swap_topk=4:swap_update=incremental:swap_patience=2:swap_min_gain=0.05 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png" \
    >/dev/null || {
    echo "not ok" 1 - "-Q center long suboptions were rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts long center suboptions"
exit 0
