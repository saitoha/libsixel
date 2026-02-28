#!/bin/sh
# TAP test: resize planner reports the scaler input pixel format.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

resize_log=$(run_img2sixel -v -=1 -W oklab -w 99% -o/dev/null <<'PPM' 2>&1
P3
4 4
255
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
PPM
) || {
    fail 1 "img2sixel failed while collecting resize planner log"
    exit 0
}
printf '%s' "${resize_log}" >&2

printf '%s' "${resize_log}" | grep -q "resize: mode=.*input=linear-f32" || {
    fail 1 "missing scaler input declaration"
    exit 0
}

pass 1 "planner reports scaler input pixelformat"
exit 0
