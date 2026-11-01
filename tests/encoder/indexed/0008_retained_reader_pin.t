#!/bin/sh
# A live baseline reader blocks commit.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0008_retained_reader_pin" || {
    echo 'not ok 1 - retained_reader_pin'
    exit 0
}
echo 'ok 1 - retained_reader_pin'
exit 0
