#!/bin/sh
# TAP test ensuring sticky_weight does not lock the whole palette.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
esc=''
sixel_record_end=''
sticky_output=''
first_frame=''
second_frame=''
rest_after_first=''
rest_after_second=''
first_palette_entry=''
second_palette_entry=''
command_status=0

echo "1..1"
set -v
set +xv

sticky_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:sticky_weight=8:scene_cut_threshold=1.0 \
        -d fs -p 16 \
        "${input_apng}"
) || command_status=$?

test "${command_status}" -eq 0 || {
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

    echo "not ok" 1 - "sticky_weight encode failed"
    exit 0
}

esc="$(printf '\033')"
sixel_record_end="${esc}\\"
first_frame="${sticky_output%%"${sixel_record_end}"*}${sixel_record_end}"
rest_after_first="${sticky_output#*"${sixel_record_end}"}"
second_frame="${rest_after_first%%"${sixel_record_end}"*}${sixel_record_end}"
rest_after_second="${rest_after_first#*"${sixel_record_end}"}"

test "${rest_after_first}" != "${sticky_output}" || {
    echo "not ok" 1 - "failed to extract first frame"
    exit 0
}

test "${rest_after_second}" != "${rest_after_first}" || {
    echo "not ok" 1 - "failed to extract second frame"
    exit 0
}

first_palette_entry="${first_frame#*#0;2;}"
second_palette_entry="${second_frame#*#0;2;}"

test "${first_palette_entry}" != "${first_frame}" || {
    echo "not ok" 1 - "failed to extract first palette entry"
    exit 0
}

test "${second_palette_entry}" != "${second_frame}" || {
    echo "not ok" 1 - "failed to extract second palette entry"
    exit 0
}

first_palette_entry="#0;2;${first_palette_entry%%#*}"
second_palette_entry="#0;2;${second_palette_entry%%#*}"

test "${first_palette_entry}" != "${second_palette_entry}" || {
    echo "not ok" 1 - "sticky_weight locked first palette entry"
    exit 0
}

echo "ok" 1 - "sticky_weight does not lock palette across frames"
exit 0
