#!/bin/sh
# Ordinary Heckbert retains source float precision through dither setup
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_heckbert" || {
    printf '\n%s\n' "not ok 1 - Ordinary Heckbert retains source float precision through dither setup"
    exit 0
}
printf '\n%s\n' "ok 1 - Ordinary Heckbert retains source float precision through dither setup"
exit 0
