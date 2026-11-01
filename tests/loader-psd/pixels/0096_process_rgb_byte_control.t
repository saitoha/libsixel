#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify process rgb byte control.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0104_process_rgb_byte_control 1>&2 || {
    echo "not ok 1 - process rgb byte control"
    exit 0
}
echo "ok 1 - process rgb byte control"
exit 0
