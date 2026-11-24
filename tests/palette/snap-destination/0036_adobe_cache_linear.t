#!/bin/sh
# adobe cache grid in linear: independent nearest/reversible values.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination grid 0 3 1 || {
    printf '\n%s\n' "not ok 1 - adobe cache grid in linear: independent nearest/reversible values"
    exit 0
}

printf '\n%s\n' "ok 1 - adobe cache grid in linear: independent nearest/reversible values"
exit 0
