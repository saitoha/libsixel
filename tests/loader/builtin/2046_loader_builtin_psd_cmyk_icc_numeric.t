#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify every CMYK PSD sample through an independent colorimetric ICC.
set -eux
echo "1..1"
set -v

SIXEL_LOADER_CMS_RENDERING_INTENT=relative! ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" "loader/0184_loader_builtin_psd_cmyk_icc_numeric" || {
    echo "not ok 1 - independent PSD CMYK ICC samples"
    exit 0
}
echo "ok 1 - independent PSD CMYK ICC samples"
exit 0
