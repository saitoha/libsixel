#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify a byte request freezes a float clustering palette to bytes and
# releases the byte component route, while other policies retain it.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0027_byte_clustering_freeze 0 1>&2 || {
    echo "not ok 1 - byte clustering palette freeze"
    exit 0
}
echo "ok 1 - byte clustering palette freeze"
exit 0
