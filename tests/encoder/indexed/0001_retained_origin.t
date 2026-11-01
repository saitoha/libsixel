#!/bin/sh
# A painted index updates only its declared plane origin.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0001_retained_origin" || {
    echo 'not ok 1 - retained_origin'
    exit 0
}
echo 'ok 1 - retained_origin'
exit 0
