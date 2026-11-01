#!/bin/sh
# Opaque black and white overwrite each other in delta output; a repeated frame preserves both pixels.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0206_delta_opaque_bw_frames || {
    echo "not ok 1 - opaque bw frames"
    exit 0
}
echo "ok 1 - opaque bw frames"
exit 0
