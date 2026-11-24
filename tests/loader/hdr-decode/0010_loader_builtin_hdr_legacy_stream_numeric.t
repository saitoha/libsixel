#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify legacy stream RLE emits exact linear float samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0107_loader_builtin_hdr_legacy_stream_numeric" || {
    printf '\n%s\n' "not ok 1 - HDR legacy stream exact float samples"
    exit 0
}

printf '\n%s\n' "ok 1 - HDR legacy stream exact float samples"
exit 0
