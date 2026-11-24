#!/bin/sh
# Extended source values survive clustering and linear working conversion before sRGB output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/0067_srgb_linear_extended_source" || {
    printf '\n%s\n' "not ok 1 - srgb_linear_extended_source"
    exit 0
}
printf '\n%s\n' "ok 1 - srgb_linear_extended_source"
exit 0
