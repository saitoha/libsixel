#!/bin/sh
# Verify default k_undither output for a 32-color indexed image.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0010_decoder_kundither_default" || {
    echo "not ok 1 - 0010_decoder_kundither_default"
    exit 0
}

echo "ok 1 - 0010_decoder_kundither_default"
exit 0
