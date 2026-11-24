#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Check the mft1 B2A matrix before input tables against known values.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0018_mft1_b2a_xyz_matrix || {
    printf '\n%s\n' "not ok 1 - mft1 B2A matrix precedes input tables"
    exit 0
}

printf '\n%s\n' "ok 1 - mft1 B2A matrix precedes input tables"
exit 0
