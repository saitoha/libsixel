#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0018_float_batch_exact" || {
    echo "not ok 1 - float batch exact"
    exit 0
}
echo "ok 1 - float batch exact"
exit 0
