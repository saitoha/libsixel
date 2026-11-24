#!/bin/sh
# ARGB gamma samples convert to P3 after alpha composition.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0067_packed_argb_p3 || {
    printf '\n%s\n' "not ok 1 - high-color packed argb p3"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color packed argb p3"
exit 0
