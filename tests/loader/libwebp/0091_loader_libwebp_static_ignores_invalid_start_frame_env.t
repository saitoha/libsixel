#!/bin/sh
# TAP test: libwebp static decode ignores invalid animation start-frame env values.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-invalid-env-default.six"
out_with_bad_env="${ARTIFACT_LOCAL_DIR}/webp-static-invalid-env-with-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static libwebp decode failed"
    exit 0
}

msg=$(set +xv; SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_webp}" \
    2>&1 >"${out_with_bad_env}") || {
    echo "not ok" 1 - "static decode failed with invalid animation start-frame env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"SIXEL_LOADER_ANIMATION_START_FRAME_NO"*)
        echo "not ok" 1 - "static decode should ignore invalid animation start-frame env"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

cmp -s "${out_default}" "${out_with_bad_env}" || {
    echo "not ok" 1 - "invalid animation start-frame env changed static output"
    exit 0
}

echo "ok" 1 - "static libwebp decode ignores invalid animation start-frame env"
exit 0
