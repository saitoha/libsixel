#!/bin/sh
# TAP test verifying unknown medoids suboption keys are rejected with candidates.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:unknown=1 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown medoids suboption key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown medoids key diagnostic"
    exit 0
}

valid_keys_line=${msg#*valid keys: }
valid_keys_line=${valid_keys_line%%.*}

test "${valid_keys_line#*algo*seed*iter*sample*histbits*point_budget*rare_keep*prune*prune_mass*}" != "${valid_keys_line}" || {
    echo "not ok" 1 - "missing medoids key candidate list"
    exit 0
}

test "${valid_keys_line#*merge*merge_oversplit*merge_lloyd*}" \
    != "${valid_keys_line}" || {
    echo "not ok" 1 - "missing medoids merge key candidate list"
    exit 0
}

echo "ok" 1 - "unknown medoids suboption key is rejected with candidates"
exit 0
