#!/bin/sh
# Policy: docs/quality/metrics.md
# Select the reference metric case-insensitively with a matching baseline.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -m mS-sSiM -b mS-sSiM:0.98 \
    "${TOP_SRCDIR}/images/snake.png" "${TOP_SRCDIR}/images/snake.png") || {
    printf '\n%s\n' "not ok 1 - MS-SSIM selection and baseline"
    exit 0
}
test "${value}" = '1.000000' || {
    printf '\n%s\n' "not ok 1 - MS-SSIM identity score"
    exit 0
}
printf '\n%s\n' "ok 1 - MS-SSIM selection and baseline"
exit 0
