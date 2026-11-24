#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Policy: docs/loader/builtin/sixel.md
# Policy: docs/decoder/pipeline/README.md
# Verify OR mode raw-index decode overlays bit-plane selectors.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0002_decoder_ormode_raw_overlay" || {
    printf '\n%s\n' "not ok 1 - 0002_decoder_ormode_raw_overlay"
    exit 0
}

printf '\n%s\n' "ok 1 - 0002_decoder_ormode_raw_overlay"
exit 0
