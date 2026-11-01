#!/bin/sh
# Accepted packets advance the baseline without display waits.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0005_retained_sequence" || {
    echo 'not ok 1 - retained_sequence'
    exit 0
}
echo 'ok 1 - retained_sequence'
exit 0
