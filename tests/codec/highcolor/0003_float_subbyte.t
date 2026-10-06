#!/bin/sh
# Float gamma representatives reach RGB101 without an intervening RGB888 store.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0003_float_subbyte || {
    echo "not ok 1 - high-color float subbyte"
    exit 0
}
echo "ok 1 - high-color float subbyte"
exit 0
