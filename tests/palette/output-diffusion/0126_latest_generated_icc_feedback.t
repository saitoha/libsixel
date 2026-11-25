#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Hold palette 2 before publication until frame 2 has selected palette 1.
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/latest_feedback \
    "icc:P${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-lut.icc" \
    >/dev/null || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP asynchronous palette scheduling unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - latest generated ICC palette and FS pixels'
    exit 0
}
printf '\n%s\n' 'ok 1 - latest generated ICC palette and FS pixels'
exit 0
