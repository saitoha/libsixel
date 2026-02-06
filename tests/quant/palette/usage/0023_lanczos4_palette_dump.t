#!/bin/sh
# Scale with Lanczos4 filter and emit palette dump.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos4-palette-dump.sixel"

if run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}" \
        >"${target_sixel}"; then
    pass 1 "Lanczos4 scaling emits palette dump"
else
    fail 1 "Lanczos4 scaling palette dump fails"
fi

exit "${status}"
