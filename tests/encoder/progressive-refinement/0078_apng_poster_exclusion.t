#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify apng poster exclusion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0105_apng_poster_exclusion" || {
    printf '\n%s\n' "not ok 1 - apng poster exclusion"
    exit 0
}
printf '\n%s\n' "ok 1 - apng poster exclusion"
exit 0
