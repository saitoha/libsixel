#!/bin/sh
# TAP test verifying unknown center suboption keys are rejected with candidates.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:unknown=1 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown center suboption key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown center key diagnostic"
    exit 0
}

valid_keys_line=${msg#*valid keys: }
valid_keys_line=${valid_keys_line%%.*}

test "${valid_keys_line#*algo*profile*seed*auto_policy*auto_fft_threshold*candidate_policy*restarts*iter*histbits*point_budget*rare_keep*prune_mass*budget_policy*budget_scale*swap_topk*swap_update*swap_patience*}" \
    != "${valid_keys_line}" || {
    echo "not ok" 1 - "missing center key candidate list"
    exit 0
}

test "${valid_keys_line#*merge*merge_oversplit*merge_lloyd*}" \
    != "${valid_keys_line}" || {
    echo "not ok" 1 - "missing center merge key candidate list"
    exit 0
}

echo "ok" 1 - "unknown center suboption key is rejected with candidates"
exit 0
