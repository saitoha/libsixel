#!/bin/sh
# TAP test verifying unknown -Q kmeans keys list histogram valid keys.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:z=soft "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown -Q key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown suboption key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

valid_keys_line=${msg#*valid keys: }
valid_keys_line=${valid_keys_line%%.*}

test "${valid_keys_line#*binning*mapping*autoratio*feedback*}" \
    != "${valid_keys_line}" || {
    echo "not ok" 1 - "missing histogram key list in unknown key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${valid_keys_line#*seed*restarts*iter*iter_max*miniter*polish_iter*}" \
    != "${valid_keys_line}" || {
    echo "not ok" 1 - "missing kmeans quality keys in unknown key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${valid_keys_line#*feedback_slots*feedback_interval*merge*merge_oversplit*merge_lloyd*}" \
    != "${valid_keys_line}" || {
    echo "not ok" 1 - "missing merge keys in unknown key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "unknown -Q key diagnostic includes histogram keys"
exit 0
