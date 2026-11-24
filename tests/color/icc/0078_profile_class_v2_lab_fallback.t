#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0078_profile_class_v2_lab_fallback || {
    printf '\n%s\n' "not ok 1 - v2 Lab ColorSpace retains the D50 Lab fallback"
    exit 0
}

printf '\n%s\n' "ok 1 - v2 Lab ColorSpace retains the D50 Lab fallback"
exit 0
