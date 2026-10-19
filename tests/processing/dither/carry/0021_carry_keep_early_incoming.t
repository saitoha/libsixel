#!/bin/sh
# Retained feedback includes correction arriving from the preceding pixel.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0021_carry_keep_early_incoming" || {
    echo "not ok 1 - keep_early_incoming"
    exit 0
}
echo "ok 1 - keep_early_incoming"
exit 0
