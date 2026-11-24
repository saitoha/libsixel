#!/bin/sh
# perturb=-0.1 is rejected
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs:perturb=-0.1 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1 >/dev/null) || status=$?
test "$status" = 2 || {
    printf '\n%s\n' "not ok 1 - invalid argument must exit 2"
    exit 0
}
case "${message}" in *-d\ perturb\ must\ be\ in\ range\ 0.0-1.0.*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing range diagnostic"
    exit 0
}

printf '\n%s\n' "ok 1 - perturb=-0.1 is rejected"
exit 0
