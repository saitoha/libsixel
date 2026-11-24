#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0020_icc_numeric_contract" || {
    printf '\n%s\n' "not ok 1 - icc_numeric_contract"
    exit 0
}
printf '\n%s\n' "ok 1 - icc_numeric_contract"
exit 0
