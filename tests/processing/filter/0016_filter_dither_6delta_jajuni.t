#!/bin/sh
# Run the Jarvis, Judice & Ninke 6delta regression via the unified runner.
# Policy: docs/functionality/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0016_filter_dither_6delta_jajuni" || {
    echo "not ok 1 - 0016_filter_dither_6delta_jajuni"
    exit 0
}

echo "ok 1 - 0016_filter_dither_6delta_jajuni"
exit 0
