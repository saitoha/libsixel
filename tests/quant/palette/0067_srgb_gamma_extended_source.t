#!/bin/sh
# Extended source values survive clustering and gamma working conversion before sRGB output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/0067_srgb_gamma_extended_source" || {
    echo "not ok 1 - srgb_gamma_extended_source"
    exit 0
}
echo "ok 1 - srgb_gamma_extended_source"
exit 0
