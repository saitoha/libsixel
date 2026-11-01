#!/bin/sh
# TAP regression test for issue #222 packed G1 encode_bytes handling.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "security/0001_issue222_encoder_encode_bytes_packed_g1" >/dev/null || {
    echo "not ok 1 - issue #222 packed G1 encode_bytes regression"
    exit 0
}

echo "ok 1 - issue #222 packed G1 encode_bytes regression"
exit 0
