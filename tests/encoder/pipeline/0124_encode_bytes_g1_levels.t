#!/bin/sh
# Decode all 2 packed G1 levels through the raw encoder API.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" filter/0010_filter_encode g1 || {
    printf '\n%s\n' 'not ok 1 - encode_bytes preserves every G1 level'
    exit 0
}
printf '\n%s\n' 'ok 1 - encode_bytes preserves every G1 level'
exit 0
