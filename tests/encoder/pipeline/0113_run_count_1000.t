#!/bin/sh
# An unlimited 1000-column run preserves its exact wire spelling.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0113_run_count_1000" || {
    printf '\n%s\n' "not ok 1 - run count 1000"
    exit 0
}
printf '\n%s\n' "ok 1 - run count 1000"
exit 0
