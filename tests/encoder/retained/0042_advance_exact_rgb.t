#!/bin/sh
# Resolved history contains transmitted colors and kept pixels.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0042_advance_exact_rgb" || {
    printf '\n%s\n' 'not ok 1 - Resolved history contains transmitted colors and kept pixels.'
    exit 0
}
printf '\n%s\n' 'ok 1 - Resolved history contains transmitted colors and kept pixels.'
exit 0
