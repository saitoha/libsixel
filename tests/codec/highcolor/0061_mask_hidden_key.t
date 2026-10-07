#!/bin/sh
# Hidden samples cannot become visible key representatives.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0061_mask_hidden_key || {
    echo "not ok 1 - high-color mask hidden key"
    exit 0
}
echo "ok 1 - high-color mask hidden key"
exit 0
