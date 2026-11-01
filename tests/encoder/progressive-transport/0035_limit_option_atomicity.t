#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify limit option atomicity.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0141_limit_option_atomicity" || {
    echo "not ok 1 - limit option atomicity"
    exit 0
}
echo "ok 1 - limit option atomicity"
exit 0
