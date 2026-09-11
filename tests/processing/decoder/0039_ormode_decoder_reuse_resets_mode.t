#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Decode OR then normal with the same decoder without leaking OR state.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0039_ormode_decoder_reuse_resets_mode" || {
    echo "not ok 1 - ormode_decoder_reuse_resets_mode"
    exit 0
}

echo "ok 1 - ormode_decoder_reuse_resets_mode"
exit 0
