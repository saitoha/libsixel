#!/bin/sh
# TAP test: an empty force-rgb value behaves like an unset variable.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-anim-lossy-default-empty-force-rgb.six"
out_empty="${ARTIFACT_LOCAL_DIR}/webp-anim-lossy-empty-force-rgb.six"

msg_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >"${out_default}") || {
    echo "not ok" 1 - "default libwebp animation decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_default}" >&2
    exit 0
}

msg_empty=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode _SIXEL_TEST_LIBWEBP_FORCE_RGB_DECODE='' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >"${out_empty}") || {
    echo "not ok" 1 - "empty-force-rgb libwebp animation decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_empty}" >&2
    exit 0
}

case "${msg_empty}" in
    *"animation background source=none"*"finalize frame_no=0 "*"finalize frame_no=1 "*)
        ;;
    *)
        echo "not ok" 1 - "empty-force-rgb trace lacks animation markers"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_empty}" >&2
        exit 0
        ;;
esac

case "${msg_empty}" in
    *"static decode path="*)
        echo "not ok" 1 - "empty-force-rgb trace reported static path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_empty}" >&2
        exit 0
        ;;
    *)
        ;;
esac

case "${msg_default}" in
    *"static decode path="*)
        echo "not ok" 1 - "default animation trace unexpectedly reported static decode path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_default}" >&2
        exit 0
        ;;
    *)
        ;;
esac

cmp -s "${out_default}" "${out_empty}" || {
    echo "not ok" 1 - "empty-force-rgb changed libwebp animation output"
    exit 0
}

echo "ok" 1 - "empty force-rgb value behaves like an unset variable"
exit 0
