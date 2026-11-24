#!/bin/sh
# An unlimited 100-column run preserves its exact wire spelling.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0111_run_count_100" || {
    printf '\n%s\n' "not ok 1 - run count 100"
    exit 0
}
printf '\n%s\n' "ok 1 - run count 100"
exit 0
