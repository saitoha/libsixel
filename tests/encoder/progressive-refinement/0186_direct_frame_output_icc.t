#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# A direct frame uses the output ICC without a presentation repair context.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    direct-delta "icc:P${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    - || {
    printf '\n%s\n' 'not ok 1 - encode_frame applies the output ICC'
    exit 0
}
printf '\n%s\n' 'ok 1 - encode_frame applies the output ICC'
exit 0
