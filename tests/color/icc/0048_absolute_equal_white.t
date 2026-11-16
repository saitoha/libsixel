#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0048_absolute_equal_white || {
    echo "not ok 1 - equal media whites retain the absolute slot 0 fallback"
    exit 0
}

echo "ok 1 - equal media whites retain the absolute slot 0 fallback"
exit 0
