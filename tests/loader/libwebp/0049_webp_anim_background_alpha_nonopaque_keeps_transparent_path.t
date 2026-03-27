#!/bin/sh
# Verify ANIM background alpha=1-254 keeps transparent path by default.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-bg112233-a80.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-alpha-nonopaque-default.six"
out_bg="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-alpha-nonopaque-bg112233.six"
keycolor_header="$(printf '\033P0;1q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "libwebp animation decode without -B failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S -B#112233 "${input_webp}" >"${out_bg}" || {
    echo "not ok" 1 - "libwebp animation decode with -B#112233 failed"
    exit 0
}

case "$(cat "${out_default}")" in
    *"${keycolor_header}"*)
        default_has_keycolor=1
        ;;
    *)
        default_has_keycolor=0
        ;;
esac

case "$(cat "${out_bg}")" in
    *"${keycolor_header}"*)
        bg_has_keycolor=1
        ;;
    *)
        bg_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "ANIM non-opaque alpha did not keep transparent-path gating"
    exit 0
}

test "${bg_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "ANIM non-opaque alpha did not keep transparent-path gating"
    exit 0
}

cmp -s "${out_default}" "${out_bg}" && {
    echo "not ok" 1 - "ANIM non-opaque alpha output unexpectedly matched -B composition"
    exit 0
}

echo "ok" 1 - "ANIM non-opaque alpha keeps transparent path until explicit -B"
exit 0
