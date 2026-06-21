#!/bin/sh
# Verify OR mode repeat spans compose each touched pixel independently.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0005_decoder_ormode_repeat_overlay" || {
    echo "not ok 1 - 0005_decoder_ormode_repeat_overlay"
    exit 0
}

echo "ok 1 - 0005_decoder_ormode_repeat_overlay"
exit 0
