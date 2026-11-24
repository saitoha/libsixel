#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0072_profile_class_unknown || {
    printf '\n%s\n' "not ok 1 - an unknown ICC class is reported unsupported"
    exit 0
}

printf '\n%s\n' "ok 1 - an unknown ICC class is reported unsupported"
exit 0
