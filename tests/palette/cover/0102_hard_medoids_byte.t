#!/bin/sh
# Verify native cover: hard medoids byte.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract wire 2 0 9 || {
    printf '\n%s\n' "not ok 1 - hard medoids byte"
    exit 0
}

printf '\n%s\n' "ok 1 - hard medoids byte"
exit 0
