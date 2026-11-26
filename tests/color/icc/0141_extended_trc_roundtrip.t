#!/bin/sh

set -eux

echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0141_extended_trc_roundtrip || {
    printf '\n%s\n' 'not ok 1 - extended TRC preserves out-of-range values'
    exit 0
}

printf '\n%s\n' 'ok 1 - extended TRC preserves out-of-range values'
exit 0
