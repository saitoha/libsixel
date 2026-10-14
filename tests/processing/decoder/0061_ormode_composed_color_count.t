#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Count composed indices that were never selected or defined individually.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0061_ormode_composed_color_count" || {
    echo "not ok 1 - composed OR color count"
    exit 0
}

echo "ok 1 - composed OR color count"
exit 0
