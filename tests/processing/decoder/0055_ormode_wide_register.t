#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Wide registers compose through all sixteen planes and final palette lookup.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0055_ormode_wide_register" || {
    echo "not ok 1 - ormode_wide_register"
    exit 0
}

echo "ok 1 - ormode_wide_register"
exit 0
