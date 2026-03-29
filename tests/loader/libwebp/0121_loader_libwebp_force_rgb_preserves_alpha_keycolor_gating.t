#!/bin/sh
# TAP test: force-rgb does not alter static lossy-alpha keycolor/background gating.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-default.six"
out_forced="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-force-rgb.six"
out_forced_bg="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-force-rgb-bg.six"
keycolor_header="$(printf '\033P0;1q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static lossy-alpha decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${input_webp}" >"${out_forced}" || {
    echo "not ok" 1 - "forced-rgb static lossy-alpha decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S -B#000 "${input_webp}" >"${out_forced_bg}" || {
    echo "not ok" 1 - "forced-rgb static lossy-alpha decode with -B#000 failed"
    exit 0
}

out_default_text=""
while IFS= read -r out_default_line || test -n "
${out_default_line}"; do
    case "${out_default_text}" in
        "")
            out_default_text=${out_default_line}
            ;;
        *)
            out_default_text="${out_default_text}
${out_default_line}"
            ;;
    esac
done < "${out_default}"
case "${out_default_text}" in
    *"${keycolor_header}"*)
        default_has_keycolor=1
        ;;
    *)
        default_has_keycolor=0
        ;;
esac

out_forced_text=""
while IFS= read -r out_forced_line || test -n "
${out_forced_line}"; do
    case "${out_forced_text}" in
        "")
            out_forced_text=${out_forced_line}
            ;;
        *)
            out_forced_text="${out_forced_text}
${out_forced_line}"
            ;;
    esac
done < "${out_forced}"
case "${out_forced_text}" in
    *"${keycolor_header}"*)
        forced_has_keycolor=1
        ;;
    *)
        forced_has_keycolor=0
        ;;
esac

out_forced_bg_text=""
while IFS= read -r out_forced_bg_line || test -n "
${out_forced_bg_line}"; do
    case "${out_forced_bg_text}" in
        "")
            out_forced_bg_text=${out_forced_bg_line}
            ;;
        *)
            out_forced_bg_text="${out_forced_bg_text}
${out_forced_bg_line}"
            ;;
    esac
done < "${out_forced_bg}"
case "${out_forced_bg_text}" in
    *"${keycolor_header}"*)
        forced_bg_has_keycolor=1
        ;;
    *)
        forced_bg_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "default static lossy-alpha output lost keycolor"
    exit 0
}

test "${forced_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "forced-rgb static lossy-alpha output lost keycolor"
    exit 0
}

test "${forced_bg_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "forced-rgb static lossy-alpha output kept keycolor with -B"
    exit 0
}

cmp -s "${out_default}" "${out_forced}" || {
    echo "not ok" 1 - "forced-rgb unexpectedly changed static lossy-alpha output without -B"
    exit 0
}

cmp -s "${out_forced}" "${out_forced_bg}" && {
    echo "not ok" 1 - "forced-rgb static lossy-alpha output unexpectedly ignored -B composition"
    exit 0
}

echo "ok" 1 - "force-rgb keeps static lossy-alpha keycolor gating and -B composition behavior"
exit 0
