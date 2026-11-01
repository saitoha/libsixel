#!/bin/sh
# Float lookup uses retained fractions near monochrome endpoints
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_mono" || {
    echo "not ok 1 - Float lookup uses retained fractions near monochrome endpoints"
    exit 0
}
echo "ok 1 - Float lookup uses retained fractions near monochrome endpoints"
exit 0
