#!/bin/sh
# TAP test: resize planner places colorspace conversion after scaling.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

resize_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -=greedy:B1 -W oklab -w 99% -o/dev/null <<'PPM' 2>&1
P3
4 4
255
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
PPM
) || {
    printf '\n%s\n' "not ok 1 - img2sixel failed while collecting resize planner log"
    exit 0
}
printf '%s' "${resize_log}" >&2

case "${resize_log}" in
    *"scale -> colorspace(post)"*) ;;
    *)
        printf '\n%s\n' "not ok 1 - missing scale -> colorspace(post)"
        exit 0
        ;;
esac

test -n "${resize_log}" || {
    printf '\n%s\n' "not ok 1 - missing scale -> colorspace(post)"
    exit 0
}

case "${resize_log}" in
    *"colorspace(post) -> dither"*) ;;
    *)
        printf '\n%s\n' "not ok 1 - missing colorspace(post) -> dither"
        exit 0
        ;;
esac

test -n "${resize_log}" || {
    printf '\n%s\n' "not ok 1 - missing colorspace(post) -> dither"
    exit 0
}

printf '\n%s\n' "ok 1 - colorspace conversion placed after scaler"

exit 0
