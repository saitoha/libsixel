#!/bin/sh
# Verify ICC palette preparation preserves the cache on failure.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    cache-retry 0 "${TOP_SRCDIR}/tests/data/color-numeric/relative-paired-lut.icc" >/dev/null || {
    printf '\n%s\n' 'not ok 1 - ICC palette preparation preserves the cache on failure'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC palette preparation preserves the cache on failure'
exit 0
