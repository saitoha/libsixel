#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify the CLI default normalizes a profiled input like explicit auto.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '1..0 # SKIP img2sixel is disabled\n'
    exit 0
}
printf '1..1\n'
set -v
input="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg"
expected=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L "builtin!" --threads=1 -d fs:scan=raster --cms-engine=auto "$input") || {
    echo "not ok 1 - CMS comparison command failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L "builtin!" --threads=1 -d fs:scan=raster --env SIXEL_LOADER_CMS_ENGINE= "$input") || {
    echo "not ok 1 - CMS comparison command failed"
    exit 0
}
disabled=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L "builtin!" --threads=1 -d fs:scan=raster --cms-engine=none "$input") || {
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
echo "ok 1 - img2sixel defaults to auto CMS"
exit 0
