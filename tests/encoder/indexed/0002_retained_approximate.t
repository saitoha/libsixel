#!/bin/sh
# An approximate keep requests refinement.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0002_retained_approximate" || {
    echo 'not ok 1 - retained_approximate'
    exit 0
}
echo 'ok 1 - retained_approximate'
exit 0
