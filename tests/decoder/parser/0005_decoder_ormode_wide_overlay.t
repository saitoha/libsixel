#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Verify OR mode wide-index decode overlays bit-plane selectors.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0003_decoder_ormode_wide_overlay" || {
    printf '\n%s\n' "not ok 1 - 0003_decoder_ormode_wide_overlay"
    exit 0
}

printf '\n%s\n' "ok 1 - 0003_decoder_ormode_wide_overlay"
exit 0
