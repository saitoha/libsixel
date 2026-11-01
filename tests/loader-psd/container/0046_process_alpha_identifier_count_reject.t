#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha identifier count reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0142_process_alpha_identifier_count_reject 1>&2 || {
    echo "not ok 1 - process alpha identifier count reject"
    exit 0
}
echo "ok 1 - process alpha identifier count reject"
exit 0
