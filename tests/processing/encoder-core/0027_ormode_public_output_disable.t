#!/bin/sh
# Policy: docs/functionality/or-mode.md
# The public output setter can turn OR mode back off.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0027_ormode_public_output_disable" || {
    echo "not ok 1 - ormode_public_output_disable"
    exit 0
}

echo "ok 1 - ormode_public_output_disable"
exit 0
