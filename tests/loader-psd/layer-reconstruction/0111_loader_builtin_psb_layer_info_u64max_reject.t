#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify PSB rejects UINT64_MAX layer-info length.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0154_loader_builtin_psb_layer_info_u64max_reject" 1>&2 || {
    echo "not ok" 1 - "PSB rejects UINT64_MAX layer-info length"
    exit 0
}

echo "ok" 1 - "PSB rejects UINT64_MAX layer-info length"

exit 0
