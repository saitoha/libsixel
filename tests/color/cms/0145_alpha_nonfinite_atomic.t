#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify alpha nonfinite atomic with independent expected values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0145_alpha_nonfinite_atomic || {
    echo 'not ok 1 - alpha_nonfinite_atomic'
    exit 0
}
echo 'ok 1 - alpha_nonfinite_atomic'
exit 0
