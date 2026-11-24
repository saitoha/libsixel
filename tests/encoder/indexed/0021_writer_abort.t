#!/bin/sh
# Abort closes framing without an acceptance receipt.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0021_writer_abort" || {
    printf '\n%s\n' 'not ok 1 - writer_abort'
    exit 0
}
printf '\n%s\n' 'ok 1 - writer_abort'
exit 0
