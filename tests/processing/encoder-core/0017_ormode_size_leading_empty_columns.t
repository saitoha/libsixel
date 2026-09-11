#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Check OR size-policy geometry and its distinct band branch.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0017_ormode_size_leading_empty_columns" || {
    echo "not ok 1 - ormode_size_leading_empty_columns"
    exit 0
}

echo "ok 1 - ormode_size_leading_empty_columns"
exit 0
