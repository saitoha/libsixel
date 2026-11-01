#!/bin/sh
# A duplicate receipt cannot commit the update twice.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0006_retained_duplicate" || {
    echo 'not ok 1 - retained_duplicate'
    exit 0
}
echo 'ok 1 - retained_duplicate'
exit 0
