#!/bin/sh
# Verify delta workspace domain.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0097_delta_workspace_domain || {
    echo "not ok 1 - delta workspace domain"
    exit 0
}
echo "ok 1 - delta workspace domain"
exit 0
