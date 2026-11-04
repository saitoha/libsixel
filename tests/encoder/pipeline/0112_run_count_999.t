#!/bin/sh
# An unlimited 999-column run preserves its exact wire spelling.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0112_run_count_999" || {
    echo "not ok 1 - run count 999"
    exit 0
}
echo "ok 1 - run count 999"
exit 0
