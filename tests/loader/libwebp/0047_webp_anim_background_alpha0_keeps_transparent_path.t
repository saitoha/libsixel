#!/bin/sh
# Verify ANIM background alpha=0 does not force background composition.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-min.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-alpha0-default.six"
out_black="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-alpha0-black.six"
keycolor_header="$(printf '\033P0;1q')"

run_img2sixel -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "libwebp animation decode without -B failed"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=none! -S -B#000 "${input_webp}" >"${out_black}" || {
    echo "not ok" 1 - "libwebp animation decode with -B#000 failed"
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

case "$(cat "${out_black}")" in
    *"${keycolor_header}"*)
        black_has_keycolor=1
        ;;
    *)
        black_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 && test "${black_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "ANIM alpha=0 did not keep transparent-path gating"
    exit 0
}

cmp -s "${out_default}" "${out_black}" && {
    echo "not ok" 1 - "ANIM alpha=0 output unexpectedly matched -B#000 composition"
    exit 0
}

echo "ok" 1 - "ANIM alpha=0 keeps transparent path until explicit -B"
exit 0
