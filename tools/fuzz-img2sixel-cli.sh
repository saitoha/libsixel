#!/bin/sh
#
# AFL wrapper for img2sixel CLI fuzzing.
#
# This script maps bytes from the mutated input file into a stable set of
# command-line arguments. By doing so, one AFL testcase exercises both:
#   1) decoder/input handling (the image payload itself), and
#   2) option parsing paths (argument values and ordering).
#
# Data flow:
#
#   AFL testcase file
#         |
#         +--> byte sampling ---> option/value selection ---> img2sixel argv
#         |
#         +-----------------------------------------------> input file path
#
# The wrapper intentionally uses a whitelist of options that are safe for
# repeated fuzzing runs and do not require external resources.

set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <afl-input-file>" >&2
    exit 2
fi

if [ "${FUZZ_IMG2SIXEL:-}" = "" ]; then
    echo "error: FUZZ_IMG2SIXEL is not set" >&2
    exit 2
fi

input_file=$1

if [ ! -r "$input_file" ]; then
    echo "error: input file is not readable: $input_file" >&2
    exit 2
fi

read_byte() {
    value=$(od -An -N1 -j"$1" -tu1 "$input_file" 2>/dev/null | tr -d ' \n\t')
    if [ -z "$value" ]; then
        value=0
    fi
    printf '%s' "$value"
}

b0=$(read_byte 0)
b1=$(read_byte 1)
b2=$(read_byte 2)
b3=$(read_byte 3)
b4=$(read_byte 4)
b5=$(read_byte 5)
b6=$(read_byte 6)
b7=$(read_byte 7)

case $((b0 % 4)) in
    0) quality=auto ;;
    1) quality=low ;;
    2) quality=high ;;
    *) quality=full ;;
esac

case $((b1 % 4)) in
    0) diffusion=auto ;;
    1) diffusion=none ;;
    2) diffusion=fs ;;
    *) diffusion=atkinson ;;
esac

case $((b2 % 4)) in
    0) palette=auto ;;
    1) palette=rgb ;;
    2) palette=hls ;;
    *) palette=rgb ;;
esac

case $((b3 % 5)) in
    0) resampling=nearest ;;
    1) resampling=bilinear ;;
    2) resampling=bicubic ;;
    3) resampling=lanczos3 ;;
    *) resampling=lanczos4 ;;
esac

case $((b4 % 3)) in
    0) loop_control=auto ;;
    1) loop_control=force ;;
    *) loop_control=disable ;;
esac

colors=$((2 + (b5 % 255)))
width=$((1 + (b6 % 160)))
height=$((1 + (b7 % 120)))

set -- "$FUZZ_IMG2SIXEL"

if [ "${FUZZ_IMG2SIXEL_BASE_ARGS:-}" != "" ]; then
    # shellcheck disable=SC2086
    set -- "$@" ${FUZZ_IMG2SIXEL_BASE_ARGS}
fi

if [ $((b0 % 2)) -eq 0 ]; then
    set -- "$@" --select-color=center
fi
if [ $((b1 % 2)) -eq 0 ]; then
    set -- "$@" --invert
fi
if [ $((b2 % 2)) -eq 0 ]; then
    set -- "$@" --ignore-delay
fi

case $((b3 % 3)) in
    0)
        set -- "$@" --quality="$quality" --diffusion="$diffusion"
        set -- "$@" --palette-type="$palette"
        set -- "$@" --resampling="$resampling"
        ;;
    1)
        set -- "$@" --resampling="$resampling" --palette-type="$palette"
        set -- "$@" --diffusion="$diffusion"
        set -- "$@" --quality="$quality"
        ;;
    *)
        set -- "$@" --palette-type="$palette" --quality="$quality"
        set -- "$@" --diffusion="$diffusion" --resampling="$resampling"
        ;;
esac

set -- "$@" --loop-control="$loop_control"
set -- "$@" --colors="$colors" --width="$width" --height="$height"
set -- "$@" "$input_file"

exec "$@"
