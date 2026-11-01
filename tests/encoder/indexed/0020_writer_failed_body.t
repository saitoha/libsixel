#!/bin/sh
# A rejected body remains sticky through end_image.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0020_writer_failed_body" || {
    echo 'not ok 1 - writer_failed_body'
    exit 0
}
echo 'ok 1 - writer_failed_body'
exit 0
