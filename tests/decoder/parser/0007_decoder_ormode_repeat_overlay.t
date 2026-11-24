#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Policy: docs/loader/builtin/sixel.md
# Policy: docs/decoder/pipeline/README.md
# Verify OR mode repeat spans compose each touched pixel independently.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0005_decoder_ormode_repeat_overlay" || {
    printf '\n%s\n' "not ok 1 - 0005_decoder_ormode_repeat_overlay"
    exit 0
}

printf '\n%s\n' "ok 1 - 0005_decoder_ormode_repeat_overlay"
exit 0
