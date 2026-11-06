#!/bin/sh
# The same resolved update cannot publish twice.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0041_advance_repeat_rejected" || {
    echo 'not ok 1 - The same resolved update cannot publish twice.'
    exit 0
}
echo 'ok 1 - The same resolved update cannot publish twice.'
exit 0
