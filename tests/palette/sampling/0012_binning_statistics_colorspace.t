#!/bin/sh
# Verify binning statistics colorspace.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0054_binning_statistics_colorspace" || {
    printf '\n%s\n' "not ok 1 - binning statistics colorspace"
    exit 0
}
printf '\n%s\n' "ok 1 - binning statistics colorspace"
exit 0
