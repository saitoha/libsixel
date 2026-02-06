#!/bin/sh
# Confirm prefixed PNG output respects explicit path.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_png="${ARTIFACT_LOCAL_DIR}/snake-explicit.png"

if run_img2sixel -o "png:${target_png}" "${snake_jpg}"; then
    if [ -s "${target_png}" ]; then
        pass 1 "prefixed PNG writes to explicit path"
    else
        fail 1 "prefixed PNG did not produce file"
    fi
else
    fail 1 "prefixed PNG conversion failed"
fi

exit "${status}"
