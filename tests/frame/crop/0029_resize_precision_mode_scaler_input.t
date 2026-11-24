#!/bin/sh
# TAP test: resize planner reports the scaler input pixel format.

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
    *"resize: mode="*"input=linear-f32"*) ;;
    *)
        printf '\n%s\n' "not ok 1 - missing scaler input declaration"
        exit 0
        ;;
esac

test -n "${resize_log}" || {
    printf '\n%s\n' "not ok 1 - missing scaler input declaration"
    exit 0
}

printf '\n%s\n' "ok 1 - planner reports scaler input pixelformat"
exit 0
