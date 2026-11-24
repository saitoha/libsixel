#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation option atomicity.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0108_presentation_option_atomicity" || {
    printf '\n%s\n' "not ok 1 - presentation option atomicity"
    exit 0
}
printf '\n%s\n' "ok 1 - presentation option atomicity"
exit 0
