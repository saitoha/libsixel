#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# The public output setter can turn OR mode back off.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0027_ormode_public_output_disable" || {
    printf '\n%s\n' "not ok 1 - ormode_public_output_disable"
    exit 0
}

printf '\n%s\n' "ok 1 - ormode_public_output_disable"
exit 0
