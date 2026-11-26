#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# A direct frame uses the output ICC without a presentation repair context.
set -eux
echo '1..1'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    direct-delta "icc:P${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    "${ARTIFACT_LOCAL_DIR}/output.six" || {
    printf '\n%s\n' 'not ok 1 - encode_bytes applies the output ICC'
    exit 0
}
printf '\n%s\n' 'ok 1 - encode_bytes applies the output ICC'
exit 0
