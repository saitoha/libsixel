#!/bin/sh
# Verify static lossy+alpha no-bg path matches forced RGB decode exactly.

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
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-default-path.six"
out_forced_rgb="${ARTIFACT_LOCAL_DIR}/webp-static-alpha-forced-rgb-path.six"
keycolor_header="$(printf '\033P0;1q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "libwebp static alpha decode with default path failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_forced_rgb}" || {
    echo "not ok" 1 - "libwebp static alpha decode with forced RGB path failed"
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

out_forced_rgb_text=""
IFS= read -r out_forced_rgb_text < "${out_forced_rgb}" || test -n "${out_forced_rgb_text}"
case "${out_forced_rgb_text}" in
    *"${keycolor_header}"*)
        forced_has_keycolor=1
        ;;
    *)
        forced_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "libwebp static alpha path lost keycolor header"
    exit 0
}

test "${forced_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "libwebp static alpha path lost keycolor header"
    exit 0
}

cmp -s "${out_default}" "${out_forced_rgb}" || {
    echo "not ok" 1 - "libwebp static alpha default path diverged from forced RGB path"
    exit 0
}

echo "ok" 1 - "libwebp static alpha default path matches forced RGB path"
exit 0
