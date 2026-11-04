#!/bin/sh
# Verify register decimal 100 packet.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0102_register_decimal_100_packet"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - register decimal 100 packet # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - register decimal 100 packet"
    exit 0
}
echo "ok 1 - register decimal 100 packet"
exit 0
