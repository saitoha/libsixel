#!/bin/sh
# Verify delta din99d compare.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0090_delta_din99d_compare || {
    printf '\n%s\n' "not ok 1 - delta din99d compare"
    exit 0
}
printf '\n%s\n' "ok 1 - delta din99d compare"
exit 0
