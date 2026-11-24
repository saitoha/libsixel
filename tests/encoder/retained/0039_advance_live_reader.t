#!/bin/sh
# A live read prevents resolved publication without mutation.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0039_advance_live_reader" || {
    printf '\n%s\n' 'not ok 1 - A live read prevents resolved publication without mutation.'
    exit 0
}
printf '\n%s\n' 'ok 1 - A live read prevents resolved publication without mutation.'
exit 0
