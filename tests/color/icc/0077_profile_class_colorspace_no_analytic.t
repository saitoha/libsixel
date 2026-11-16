#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0077_profile_class_colorspace_no_analytic || {
    echo "not ok 1 - ColorSpace requires an explicit PCS direction"
    exit 0
}

echo "ok 1 - ColorSpace requires an explicit PCS direction"
exit 0
