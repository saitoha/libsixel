#!/bin/sh
# Retained feedback includes correction arriving from the preceding pixel.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0022_carry_keep_lookup_incoming" || {
    echo "not ok 1 - keep_lookup_incoming"
    exit 0
}
echo "ok 1 - keep_lookup_incoming"
exit 0
