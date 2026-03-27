#!/bin/sh
# Verify static alpha WebP keeps keycolor without -B and composites with -B.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-default.six"
out_default_cms="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-default-cms.six"
out_black="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-bg-black.six"
out_white="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-bg-white.six"
keycolor_header="$(printf '\033P0;1q')"

run_img2sixel -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "libwebp static alpha decode without -B failed"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=auto! -S "${input_webp}" >"${out_default_cms}" || {
    echo "not ok" 1 - "libwebp static alpha decode without -B failed (cms=auto)"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=none! -S -B#000 "${input_webp}" >"${out_black}" || {
    echo "not ok" 1 - "libwebp static alpha decode with -B#000 failed"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=none! -S -B#fff "${input_webp}" >"${out_white}" || {
    echo "not ok" 1 - "libwebp static alpha decode with -B#fff failed"
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

case "$(cat "${out_default_cms}")" in
    *"${keycolor_header}"*)
        cms_has_keycolor=1
        ;;
    *)
        cms_has_keycolor=0
        ;;
esac

case "$(cat "${out_white}")" in
    *"${keycolor_header}"*)
        white_has_keycolor=1
        ;;
    *)
        white_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 &&
    test "${cms_has_keycolor}" -eq 1 &&
    test "${black_has_keycolor}" -eq 0 &&
    test "${white_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "libwebp static alpha keycolor gating mismatch"
    exit 0
}

cmp -s "${out_black}" "${out_white}" && {
    echo "not ok" 1 - "libwebp static alpha background composition did not change with -B"
    exit 0
}

echo "ok" 1 - "libwebp static alpha keycolor and -B composition are both active"
exit 0
