#!/bin/sh
# Low-level dither settings survive another instance changing and being destroyed.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0036_dither_instances" || {
    echo "not ok 1 - dither_instances"
    exit 0
}
echo "ok 1 - dither_instances"
exit 0
