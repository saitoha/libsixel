#!/bin/sh
# Gray-alpha honors the background through CIELAB and P3.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0070_packed_ga_cielab || {
    echo "not ok 1 - high-color packed ga cielab"
    exit 0
}
echo "ok 1 - high-color packed ga cielab"
exit 0
