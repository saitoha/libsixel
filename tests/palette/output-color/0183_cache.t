#!/bin/sh
# Unchanged palette application preserves prepared output values
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_cache" || {
    printf '\n%s\n' "not ok 1 - Unchanged palette application preserves prepared output values"
    exit 0
}
printf '\n%s\n' "ok 1 - Unchanged palette application preserves prepared output values"
exit 0
