#!/bin/sh
# Verify palette protected swap.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0006_palette_protected_swap" || {
    echo "not ok 1 - palette protected swap"
    exit 0
}
echo "ok 1 - palette protected swap"
exit 0
