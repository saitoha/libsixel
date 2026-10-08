#!/bin/sh
# Uncached serialization rounds the retained float coordinates
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_wire" || {
    echo "not ok 1 - Uncached serialization rounds the retained float coordinates"
    exit 0
}
echo "ok 1 - Uncached serialization rounds the retained float coordinates"
exit 0
