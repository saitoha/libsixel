#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha mt32 psb reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0109_process_alpha_mt32_psb_reject 1>&2 || {
    echo "not ok 1 - process alpha mt32 psb reject"
    exit 0
}
echo "ok 1 - process alpha mt32 psb reject"
exit 0
