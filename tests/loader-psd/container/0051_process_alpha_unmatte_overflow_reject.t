#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha unmatte overflow reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0149_process_alpha_unmatte_overflow_reject 1>&2 || {
    echo "not ok 1 - process alpha unmatte overflow reject"
    exit 0
}
echo "ok 1 - process alpha unmatte overflow reject"
exit 0
