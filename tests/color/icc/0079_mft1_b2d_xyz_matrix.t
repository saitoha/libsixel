#!/bin/sh
# Policy: docs/loader/builtin-cms.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0079_mft1_b2d_xyz_matrix || {
    echo "not ok 1 - mft1 B2D applies the XYZ matrix before input tables"
    exit 0
}

echo "ok 1 - mft1 B2D applies the XYZ matrix before input tables"
exit 0
