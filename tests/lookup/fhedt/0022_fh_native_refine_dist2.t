#!/bin/sh
# Verify fhedt native refine dist2.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0012_fh_native_refine_dist2 || {
    echo "not ok 1 - fhedt native refine dist2"
    exit 0
}

echo "ok 1 - fhedt native refine dist2"
exit 0
