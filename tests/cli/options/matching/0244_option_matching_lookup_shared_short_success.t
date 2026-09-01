#!/bin/sh
# TAP test verifying the shared lookup suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" '-~5bit:S1' \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
    echo "not ok" 1 - "lookup shared_instance short form was rejected"
    exit 0
}

echo "ok" 1 - "lookup shared_instance short form is accepted"
exit 0
