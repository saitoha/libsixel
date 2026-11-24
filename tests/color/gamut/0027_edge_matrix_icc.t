#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0027_edge_matrix_icc || {
    printf '\n%s\n' "not ok 1 - edge matrix icc"
    exit 0
}
printf '\n%s\n' "ok 1 - edge matrix icc"
exit 0
