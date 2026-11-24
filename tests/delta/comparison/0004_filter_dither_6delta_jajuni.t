#!/bin/sh
# Run the Jarvis, Judice & Ninke 6delta regression via the unified runner.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0016_filter_dither_6delta_jajuni" || {
    printf '\n%s\n' "not ok 1 - 0016_filter_dither_6delta_jajuni"
    exit 0
}

printf '\n%s\n' "ok 1 - 0016_filter_dither_6delta_jajuni"
exit 0
