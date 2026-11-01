#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha rle numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0151_process_alpha_rle_numeric 1>&2 || {
    echo "not ok 1 - process alpha rle numeric"
    exit 0
}
echo "ok 1 - process alpha rle numeric"
exit 0
