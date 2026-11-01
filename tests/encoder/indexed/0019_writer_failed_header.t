#!/bin/sh
# A rejected header never issues an acceptance receipt.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0019_writer_failed_header" || {
    echo 'not ok 1 - writer_failed_header'
    exit 0
}
echo 'ok 1 - writer_failed_header'
exit 0
