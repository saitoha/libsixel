#!/bin/sh
# TAP test: static alpha start-frame keeps keycolor unless -B is specified.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-min.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-start-frame-default.six"
out_start1="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-start-frame-1.six"
out_start1_bg="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-start-frame-1-bg.six"
keycolor_header="$(printf '\033P0;1q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static alpha decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -Llibwebp:cms_engine=none! -S \
    "${input_webp}" >"${out_start1}" || {
    echo "not ok" 1 - "static alpha decode with --start-frame=1 failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -Llibwebp:cms_engine=none! -S -B#000 \
    "${input_webp}" >"${out_start1_bg}" || {
    echo "not ok" 1 - "static alpha decode with --start-frame=1 -B#000 failed"
    exit 0
}

cmp -s "${out_default}" "${out_start1}" && {
    echo "not ok" 1 - "start-frame=1 did not change static alpha frame selection"
    exit 0
}

out_start1_text=""
while IFS= read -r out_start1_line || test -n "
${out_start1_line}"; do
    case "${out_start1_text}" in
        "")
            out_start1_text=${out_start1_line}
            ;;
        *)
            out_start1_text="${out_start1_text}
${out_start1_line}"
            ;;
    esac
done < "${out_start1}"
case "${out_start1_text}" in
    *"${keycolor_header}"*)
        start1_has_keycolor=1
        ;;
    *)
        start1_has_keycolor=0
        ;;
esac

out_start1_bg_text=""
while IFS= read -r out_start1_bg_line || test -n "
${out_start1_bg_line}"; do
    case "${out_start1_bg_text}" in
        "")
            out_start1_bg_text=${out_start1_bg_line}
            ;;
        *)
            out_start1_bg_text="${out_start1_bg_text}
${out_start1_bg_line}"
            ;;
    esac
done < "${out_start1_bg}"
case "${out_start1_bg_text}" in
    *"${keycolor_header}"*)
        start1_bg_has_keycolor=1
        ;;
    *)
        start1_bg_has_keycolor=0
        ;;
esac

test "${start1_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "start-frame alpha output lost keycolor without -B"
    exit 0
}

test "${start1_bg_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "start-frame alpha output kept keycolor even with -B"
    exit 0
}

cmp -s "${out_start1}" "${out_start1_bg}" && {
    echo "not ok" 1 - "start-frame alpha output unexpectedly matched -B composition"
    exit 0
}

echo "ok" 1 - "static alpha start-frame keeps keycolor until explicit -B"
exit 0
