#!/bin/sh
# Verify delta fixed fs diffuse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0048_delta_fixed_fs_diffuse || {
    echo "not ok 1 - delta fixed fs diffuse"
    exit 0
}
echo "ok 1 - delta fixed fs diffuse"
exit 0
