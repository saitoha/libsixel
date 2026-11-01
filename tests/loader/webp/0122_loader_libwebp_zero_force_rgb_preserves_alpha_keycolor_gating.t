#!/bin/sh
# TAP test: force-rgb value 0 preserves lossy-alpha keycolor/background gating.

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
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-default-zero.six"
out_zero="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-zero-force-rgb.six"
out_zero_bg="${ARTIFACT_LOCAL_DIR}/webp-static-lossy-alpha-keycolor-zero-force-rgb-bg.six"
keycolor_header="$(printf '\033P0;1q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -A composite -L libwebp! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static lossy-alpha decode failed"
    exit 0
}

_SIXEL_TEST_LIBWEBP_FORCE_RGB_DECODE=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -A composite -L libwebp! -S "${input_webp}" >"${out_zero}" || {
    echo "not ok" 1 - "zero-force-rgb static lossy-alpha decode failed"
    exit 0
}

_SIXEL_TEST_LIBWEBP_FORCE_RGB_DECODE=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -A composite -L libwebp! -S -B#000 "${input_webp}" >"${out_zero_bg}" || {
    echo "not ok" 1 - "zero-force-rgb lossy-alpha decode with -B#000 failed"
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

out_zero_text=""
IFS= read -r out_zero_text < "${out_zero}" || test -n "${out_zero_text}"
case "${out_zero_text}" in
    *"${keycolor_header}"*)
        zero_has_keycolor=1
        ;;
    *)
        zero_has_keycolor=0
        ;;
esac

out_zero_bg_text=""
IFS= read -r out_zero_bg_text < "${out_zero_bg}" || test -n "${out_zero_bg_text}"
case "${out_zero_bg_text}" in
    *"${keycolor_header}"*)
        zero_bg_has_keycolor=1
        ;;
    *)
        zero_bg_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "default static lossy-alpha output lost keycolor"
    exit 0
}

test "${zero_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "zero-force-rgb static lossy-alpha output lost keycolor"
    exit 0
}

test "${zero_bg_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "zero-force-rgb output kept keycolor with -B"
    exit 0
}

cmp -s "${out_default}" "${out_zero}" || {
    echo "not ok" 1 - "zero-force-rgb changed lossy-alpha output without -B"
    exit 0
}

cmp -s "${out_zero}" "${out_zero_bg}" && {
    echo "not ok" 1 - "zero-force-rgb output ignored -B composition"
    exit 0
}

echo "ok" 1 - "force-rgb value 0 preserves alpha gating and composition"
exit 0
