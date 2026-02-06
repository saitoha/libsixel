#!/bin/sh
# Scale with Lanczos2 filter while limiting palette size.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos2-palette-limit.sixel"

if run_img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 "${snake_jpg}" \
        >"${target_sixel}"; then
    pass 1 "Lanczos2 scaling with palette limit succeeds"
else
    fail 1 "Lanczos2 scaling with palette limit fails"
fi

exit "${status}"
