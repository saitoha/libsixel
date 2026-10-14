#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Composite registers combine all selected bits, including bit 7.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0054_ormode_composite_register" || {
    echo "not ok 1 - ormode_composite_register"
    exit 0
}

echo "ok 1 - ormode_composite_register"
exit 0
