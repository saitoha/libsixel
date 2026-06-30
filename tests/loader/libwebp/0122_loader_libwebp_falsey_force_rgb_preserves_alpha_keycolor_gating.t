#!/bin/sh
# TAP test: falsey force-rgb token does not alter static lossy-alpha keycolor/background gating.

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
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-default-falsey.six"
out_falsey="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-falsey-force-rgb.six"
out_falsey_bg="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-falsey-force-rgb-bg.six"
keycolor_header="$(printf '\033P0;0q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static lossy-alpha decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=n \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${input_webp}" >"${out_falsey}" || {
    echo "not ok" 1 - "falsey-force-rgb static lossy-alpha decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=n \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S -B#000 "${input_webp}" >"${out_falsey_bg}" || {
    echo "not ok" 1 - "falsey-force-rgb static lossy-alpha decode with -B#000 failed"
    exit 0
}

set +x
out_default_text=""
IFS= read -r out_default_text < "${out_default}" || test -n "${out_default_text}"
case "${out_default_text}" in
    *"${keycolor_header}"*)
        default_has_keycolor=1
        ;;
    *)
        default_has_keycolor=0
        ;;
esac

out_falsey_text=""
IFS= read -r out_falsey_text < "${out_falsey}" || test -n "${out_falsey_text}"
case "${out_falsey_text}" in
    *"${keycolor_header}"*)
        falsey_has_keycolor=1
        ;;
    *)
        falsey_has_keycolor=0
        ;;
esac

out_falsey_bg_text=""
IFS= read -r out_falsey_bg_text < "${out_falsey_bg}" || test -n "${out_falsey_bg_text}"
case "${out_falsey_bg_text}" in
    *"${keycolor_header}"*)
        falsey_bg_has_keycolor=1
        ;;
    *)
        falsey_bg_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "default static lossy-alpha output lost keycolor"
    exit 0
}

test "${falsey_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "falsey-force-rgb static lossy-alpha output lost keycolor"
    exit 0
}

test "${falsey_bg_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "falsey-force-rgb static lossy-alpha output kept keycolor with -B"
    exit 0
}

cmp -s "${out_default}" "${out_falsey}" || {
    echo "not ok" 1 - "falsey-force-rgb unexpectedly changed static lossy-alpha output without -B"
    exit 0
}

cmp -s "${out_falsey}" "${out_falsey_bg}" && {
    echo "not ok" 1 - "falsey-force-rgb static lossy-alpha output unexpectedly ignored -B composition"
    exit 0
}

echo "ok" 1 - "falsey force-rgb token keeps static lossy-alpha keycolor gating and -B composition behavior"
exit 0
