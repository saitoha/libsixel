#!/bin/sh
# TAP test verifying removed medoids prune key is absent from key candidates.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:prune=elkan \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "removed medoids prune key unexpectedly succeeded"
    exit 0
}

valid_keys_line=${msg#*valid keys: }
valid_keys_line=${valid_keys_line%%.*}

test "${valid_keys_line#*, prune,*}" = "${valid_keys_line}" || {
    echo "not ok" 1 - "removed medoids prune key is still listed"
    exit 0
}

test "${valid_keys_line#*prune_mass*}" != "${valid_keys_line}" || {
    echo "not ok" 1 - "medoids prune_mass key missing from candidates"
    exit 0
}

echo "ok" 1 - "removed medoids prune key is not listed in candidates"
exit 0
