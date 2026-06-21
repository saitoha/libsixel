#!/bin/sh
# Verify OR mode wide-index decode overlays bit-plane selectors.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0003_decoder_ormode_wide_overlay" || {
    echo "not ok 1 - 0003_decoder_ormode_wide_overlay"
    exit 0
}

echo "ok 1 - 0003_decoder_ormode_wide_overlay"
exit 0
