#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0074_profile_class_colorspace_matrix || {
    printf '\n%s\n' "not ok 1 - ColorSpace matrix tags cannot supply a path"
    exit 0
}

printf '\n%s\n' "ok 1 - ColorSpace matrix tags cannot supply a path"
exit 0
