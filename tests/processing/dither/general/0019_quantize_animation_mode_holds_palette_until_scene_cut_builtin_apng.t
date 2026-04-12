#!/bin/sh
# TAP test ensuring animation_mode keeps palette fixed below scene-cut limit.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"
esc=''
sixel_record_end=''
locked_output=''
first_frame=''
second_frame=''
rest_after_first=''
rest_after_second=''
first_palette_prefix=''
second_palette_prefix=''
frame_scan=''
frame_token=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -Qauto -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

locked_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:animation_mode=1:scene_cut_threshold=1.0 \
        -d fs -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "animation_mode lock encode failed"
    exit 0
}

esc="$(printf '\033')"
sixel_record_end="${esc}\\"
first_frame="${locked_output%%"${sixel_record_end}"*}${sixel_record_end}"
rest_after_first="${locked_output#*"${sixel_record_end}"}"
second_frame="${rest_after_first%%"${sixel_record_end}"*}${sixel_record_end}"
rest_after_second="${rest_after_first#*"${sixel_record_end}"}"

test "${rest_after_first}" != "${locked_output}" || {
    echo "not ok" 1 - "failed to extract first frame"
    exit 0
}

test "${rest_after_second}" != "${rest_after_first}" || {
    echo "not ok" 1 - "failed to extract second frame"
    exit 0
}

first_palette_prefix=''
frame_scan="${first_frame}"
while :; do
    case "${frame_scan}" in
        *'#'*)
            frame_token=${frame_scan#*#}
            frame_token=${frame_token%%#*}
            case "${frame_token}" in
                *';2;'*)
                    first_palette_prefix="${first_palette_prefix}#${frame_token}"
                    ;;
                *)
                    break
                    ;;
            esac
            frame_scan=${frame_scan#*#}
            case "${frame_scan}" in
                *'#'*)
                    frame_scan=${frame_scan#*#}
                    ;;
                *)
                    break
                    ;;
            esac
            ;;
        *)
            break
            ;;
    esac
done

second_palette_prefix=''
frame_scan="${second_frame}"
while :; do
    case "${frame_scan}" in
        *'#'*)
            frame_token=${frame_scan#*#}
            frame_token=${frame_token%%#*}
            case "${frame_token}" in
                *';2;'*)
                    second_palette_prefix="${second_palette_prefix}#${frame_token}"
                    ;;
                *)
                    break
                    ;;
            esac
            frame_scan=${frame_scan#*#}
            case "${frame_scan}" in
                *'#'*)
                    frame_scan=${frame_scan#*#}
                    ;;
                *)
                    break
                    ;;
            esac
            ;;
        *)
            break
            ;;
    esac
done

test "${first_palette_prefix}" = "${second_palette_prefix}" || {
    echo "not ok" 1 - "animation_mode changed palette below scene cut threshold"
    exit 0
}

echo "ok" 1 - "animation_mode keeps palette fixed below scene cut threshold"
exit 0
