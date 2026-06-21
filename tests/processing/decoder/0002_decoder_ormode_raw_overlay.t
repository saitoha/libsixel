#!/bin/sh
# Verify OR mode raw-index decode overlays bit-plane selectors.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0002_decoder_ormode_raw_overlay" || {
    echo "not ok 1 - 0002_decoder_ormode_raw_overlay"
    exit 0
}

echo "ok 1 - 0002_decoder_ormode_raw_overlay"
exit 0
