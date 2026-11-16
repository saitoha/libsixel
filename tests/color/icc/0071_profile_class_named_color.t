#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0071_profile_class_named_color || {
    echo "not ok 1 - a NamedColor profile is rejected as a PCS endpoint"
    exit 0
}

echo "ok 1 - a NamedColor profile is rejected as a PCS endpoint"
exit 0
