#!/bin/sh
# Explicit identical byte assignment replaces float precision
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_setter_same" || {
    echo "not ok 1 - Explicit identical byte assignment replaces float precision"
    exit 0
}
echo "ok 1 - Explicit identical byte assignment replaces float precision"
exit 0
