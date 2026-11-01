#!/bin/sh
# Verify delta byte bluenoise dither keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0070_delta_byte_bluenoise_dither_keep || {
    echo "not ok 1 - delta byte bluenoise dither keep"
    exit 0
}
echo "ok 1 - delta byte bluenoise dither keep"
exit 0
