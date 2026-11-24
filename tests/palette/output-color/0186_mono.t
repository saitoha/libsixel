#!/bin/sh
# Float lookup uses retained fractions near monochrome endpoints
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_mono" || {
    printf '\n%s\n' "not ok 1 - Float lookup uses retained fractions near monochrome endpoints"
    exit 0
}
printf '\n%s\n' "ok 1 - Float lookup uses retained fractions near monochrome endpoints"
exit 0
