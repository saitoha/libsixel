#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Policy: docs/decoder/pipeline/README.md
# Verify OR mode decoding through the deprecated compatibility API.

set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0006_decoder_ormode_legacy_api" || status=$?

test "${status}" != 77 || {
    echo "ok 1 # SKIP compiler cannot suppress deprecated API diagnostics"
    exit 0
}
test "${status}" = 0 || {
    echo "not ok 1 - legacy OR decode failed"
    exit 0
}

echo "ok 1 - 0006_decoder_ormode_legacy_api"
exit 0
