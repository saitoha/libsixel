#!/bin/sh
# Validate GIF conversion with scaling and filters.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_gif="${top_srcdir}/tests/data/inputs/small.gif"
target_sixel="${ARTIFACT_LOCAL_DIR}/snake-gif.sixel"

if run_img2sixel -w105% -h100 -B"#000000000" -rne <"${snake_gif}" \
    >"${target_sixel}"; then
    pass 1 "GIF conversion with filters succeeded"
else
    fail 1 "GIF conversion with filters failed"
fi

exit "${status}"
