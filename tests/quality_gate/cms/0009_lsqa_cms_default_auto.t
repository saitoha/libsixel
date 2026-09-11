#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify the CLI default normalizes a profiled input like explicit auto.
set -eux
printf '1..1\n'
set -v
input="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg"
expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L "builtin!" -m MS-SSIM --cms-engine=auto "$input" "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - CMS comparison command failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L "builtin!" -m MS-SSIM --env SIXEL_LOADER_CMS_ENGINE= "$input" "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - CMS comparison command failed"
    exit 0
}
disabled=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L "builtin!" -m MS-SSIM --cms-engine=none "$input" "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - CMS comparison command failed"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - default CMS must match auto and differ from none"
    exit 0
}
test "$actual" != "$disabled" || {
    echo "not ok 1 - default CMS must match auto and differ from none"
    exit 0
}
echo "ok 1 - lsqa defaults to auto CMS"
exit 0
