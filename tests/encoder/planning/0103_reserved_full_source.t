#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Explicit full-frame sampling uses only the independent source.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0103_reserved_full_source" || {
    echo "not ok 1 - reserved full source"
    exit 0
}
echo "ok 1 - reserved full source"
exit 0
