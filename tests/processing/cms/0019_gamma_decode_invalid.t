#!/bin/sh
# Verify gamma decode invalid.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0019_gamma_decode_invalid" || {
    echo "not ok 1 - gamma decode invalid"
    exit 0
}
echo "ok 1 - gamma decode invalid"
exit 0
