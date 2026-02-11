#!/bin/sh
# Inspect Sixel metadata successfully.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${images_dir}/map8.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection.txt"

if run_img2sixel -I "${snake_six}" >"${target_txt}"; then
    pass 1 "Sixel metadata inspection succeeds"
else
    fail 1 "Sixel metadata inspection fails"
fi

exit "${status}"
