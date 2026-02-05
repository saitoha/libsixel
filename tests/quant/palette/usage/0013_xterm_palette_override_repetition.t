#!/bin/sh
# Ensure xterm palette overrides can repeat safely.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_pbm="${TOP_SRCDIR}/tests/data/inputs/snake_64.pbm"
target_sixel="${ARTIFACT_LOCAL_DIR}/xterm-override.sixel"

if run_img2sixel -7 -w100 -h100 -bxterm16 -B"#aB3" -B"#aB3" \
        "${snake_pbm}" >"${target_sixel}"; then
    pass 1 "xterm palette overrides repeat"
else
    fail 1 "xterm palette overrides fail"
fi

exit "${status}"
