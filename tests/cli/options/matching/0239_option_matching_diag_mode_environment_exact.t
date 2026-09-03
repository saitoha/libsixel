#!/bin/sh
# Verify SIXEL_DIAG_MODE keeps its exact lowercase environment contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

prefix_status=0
case_status=0
prefix_message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=c \
    -d st "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || prefix_status=$?
case_message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=CODE \
    -d st "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || case_status=$?

test "${prefix_status}" -eq 2 || {
    echo "not ok" 1 - "diagnostics prefix exit status mismatch"
    exit 0
}

test "${case_status}" -eq 2 || {
    echo "not ok" 1 - "diagnostics case exit status mismatch"
    exit 0
}

test "${prefix_message#*\(matches:*}" != "${prefix_message}" || {
    echo "not ok" 1 - "diagnostics environment accepted a prefix"
    exit 0
}

test "${case_message#*\(matches:*}" != "${case_message}" || {
    echo "not ok" 1 - "diagnostics environment ignored case"
    exit 0
}

echo "ok" 1 - "diagnostics environment matching remains exact"
exit 0
