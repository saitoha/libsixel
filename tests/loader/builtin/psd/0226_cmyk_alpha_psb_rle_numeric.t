#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify cmyk alpha psb rle numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0226_cmyk_alpha_psb_rle_numeric 1>&2 || {
    echo "not ok 1 - cmyk alpha psb rle numeric"
    exit 0
}
echo "ok 1 - cmyk alpha psb rle numeric"
exit 0
