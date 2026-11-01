#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify process rgb savedmask numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0090_process_rgb_savedmask_numeric 1>&2 || {
    echo "not ok 1 - process rgb savedmask numeric"
    exit 0
}
echo "ok 1 - process rgb savedmask numeric"
exit 0
