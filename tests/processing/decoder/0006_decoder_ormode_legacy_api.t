#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Policy: docs/functionality/decoding-pipeline.md
# Verify OR mode decoding through the deprecated compatibility API.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0006_decoder_ormode_legacy_api" || {
    echo "not ok 1 - 0006_decoder_ormode_legacy_api"
    exit 0
}

echo "ok 1 - 0006_decoder_ormode_legacy_api"
exit 0
