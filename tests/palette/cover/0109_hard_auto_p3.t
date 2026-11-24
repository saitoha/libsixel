#!/bin/sh
# Verify native cover: hard auto p3.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract wire 4 0 3 || {
    printf '\n%s\n' "not ok 1 - hard auto p3"
    exit 0
}

printf '\n%s\n' "ok 1 - hard auto p3"
exit 0
