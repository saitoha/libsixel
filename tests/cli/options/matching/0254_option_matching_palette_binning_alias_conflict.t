#!/bin/sh
# Verify conflicting top-level and legacy binning values reject either order.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status_forward=0
status_reverse=0
message_forward=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --palette-binning=hard -Qkmeans:binning=soft 2>&1) || status_forward=$?
message_reverse=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:binning=soft --palette-binning=hard 2>&1) || status_reverse=$?

test "${status_forward}" -eq 2 -a "${status_reverse}" -eq 2 || {
    echo "not ok 1 - binning alias conflict depended on option order"
    exit 0
}

test "${message_forward#*conflicts with the deprecated*}" != \
    "${message_forward}" -a \
    "${message_reverse#*conflicts with the deprecated*}" != \
    "${message_reverse}" || {
    echo "not ok 1 - binning alias conflict diagnostic is missing"
    exit 0
}

echo "ok 1 - binning alias conflicts reject both option orders"
exit 0
