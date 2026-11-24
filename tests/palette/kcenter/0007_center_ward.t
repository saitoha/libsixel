#!/bin/sh
# Ward merge retains signed independent means.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/center_ward" || {
    printf '\n%s\n' "not ok 1 - Ward merge retains signed independent means"
    exit 0
}
printf '\n%s\n' "ok 1 - Ward merge retains signed independent means"
exit 0
