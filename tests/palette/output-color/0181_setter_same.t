#!/bin/sh
# Explicit identical byte assignment replaces float precision
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_setter_same" || {
    printf '\n%s\n' "not ok 1 - Explicit identical byte assignment replaces float precision"
    exit 0
}
printf '\n%s\n' "ok 1 - Explicit identical byte assignment replaces float precision"
exit 0
