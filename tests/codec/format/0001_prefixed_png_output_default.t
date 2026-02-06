#!/bin/sh
# Confirm prefixed PNG output is created via png: scheme.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
prefixed_png="${ARTIFACT_LOCAL_DIR}/snake-prefixed.png"

if run_img2sixel -o "png:${prefixed_png}" "${snake_jpg}"; then
    if [ -s "${prefixed_png}" ]; then
        pass 1 "prefixed PNG output created"
    else
        fail 1 "prefixed PNG output missing"
    fi
else
    fail 1 "prefixed PNG conversion failed"
fi

exit "${status}"
