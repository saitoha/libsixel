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

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-default.six"
out_default_cms="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-default-cms.six"
out_black="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-bg-black.six"
out_white="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-keycolor-bg-white.six"
keycolor_header="$(printf '\033P0;1q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "libwebp static alpha decode without -B failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! -S "${input_webp}" >"${out_default_cms}" || {
    echo "not ok" 1 - "libwebp static alpha decode without -B failed (cms=auto)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S -B#000 "${input_webp}" >"${out_black}" || {
    echo "not ok" 1 - "libwebp static alpha decode with -B#000 failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S -B#fff "${input_webp}" >"${out_white}" || {
    echo "not ok" 1 - "libwebp static alpha decode with -B#fff failed"
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

out_black_text=""
while IFS= read -r out_black_line || test -n "
${out_black_line}"; do
    case "${out_black_text}" in
        "")
            out_black_text=${out_black_line}
            ;;
        *)
            out_black_text="${out_black_text}
${out_black_line}"
            ;;
    esac
done < "${out_black}"
case "${out_black_text}" in
    *"${keycolor_header}"*)
        black_has_keycolor=1
        ;;
    *)
        black_has_keycolor=0
        ;;
esac

out_default_cms_text=""
while IFS= read -r out_default_cms_line || test -n "
${out_default_cms_line}"; do
    case "${out_default_cms_text}" in
        "")
            out_default_cms_text=${out_default_cms_line}
            ;;
        *)
            out_default_cms_text="${out_default_cms_text}
${out_default_cms_line}"
            ;;
    esac
done < "${out_default_cms}"
case "${out_default_cms_text}" in
    *"${keycolor_header}"*)
        cms_has_keycolor=1
        ;;
    *)
        cms_has_keycolor=0
        ;;
esac

out_white_text=""
while IFS= read -r out_white_line || test -n "
${out_white_line}"; do
    case "${out_white_text}" in
        "")
            out_white_text=${out_white_line}
            ;;
        *)
            out_white_text="${out_white_text}
${out_white_line}"
            ;;
    esac
done < "${out_white}"
case "${out_white_text}" in
    *"${keycolor_header}"*)
        white_has_keycolor=1
        ;;
    *)
        white_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "libwebp static alpha keycolor gating mismatch"
    exit 0
}

test "${cms_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "libwebp static alpha keycolor gating mismatch"
    exit 0
}

test "${black_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "libwebp static alpha keycolor gating mismatch"
    exit 0
}

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
