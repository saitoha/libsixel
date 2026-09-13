#!/bin/sh
# perturb=1.5 is rejected
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs:perturb=1.5 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1 >/dev/null) || status=$?
test "$status" = 2 || {
    echo "not ok 1 - invalid argument must exit 2"
    exit 0
}
test "${message#*-d perturb must be in range 0.0-1.0.}" != "$message" || {
    echo "not ok 1 - missing range diagnostic"
    exit 0
}

echo "ok 1 - perturb=1.5 is rejected"
exit 0
