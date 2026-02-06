#!/bin/sh
# Ensure stacked palette files are handled correctly.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

map8_six="${images_dir}/map8.six"
snake_six="${top_srcdir}/tests/data/inputs/snake_64.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/stacked-palettes.sixel"

if run_img2sixel -m "${map8_six}" -m "${map8_six}" "${snake_six}" >"${target_sixel}"; then
    pass 1 "stacked palette files handled"
else
    fail 1 "stacked palette files fail"
fi

exit "${status}"
