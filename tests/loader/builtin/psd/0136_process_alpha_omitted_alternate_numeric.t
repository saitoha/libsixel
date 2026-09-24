#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify process alpha omitted alternate numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0136_process_alpha_omitted_alternate_numeric 1>&2 || {
    echo "not ok 1 - process alpha omitted alternate numeric"
    exit 0
}
echo "ok 1 - process alpha omitted alternate numeric"
exit 0
