#!/bin/sh
# A failed header drains writer state for the next image.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0022_writer_reuse_after_failure" || {
    echo 'not ok 1 - writer_reuse_after_failure'
    exit 0
}
echo 'ok 1 - writer_reuse_after_failure'
exit 0
