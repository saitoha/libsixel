#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.
# Policy: docs/encoder/palette-construction/merge-policy/README.md

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0006_filter_final_merge" || {
    printf '\n%s\n' "not ok 1 - 0006_filter_final_merge"
    exit 0
}

printf '\n%s\n' "ok 1 - 0006_filter_final_merge"
exit 0
