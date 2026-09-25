#!/bin/sh
# Policy: docs/writers/png.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0020_icc_numeric_contract" || {
    echo "not ok 1 - icc_numeric_contract"
    exit 0
}
echo "ok 1 - icc_numeric_contract"
exit 0
