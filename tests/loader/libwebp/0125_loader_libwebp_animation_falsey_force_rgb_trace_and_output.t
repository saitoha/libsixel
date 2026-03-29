#!/bin/sh
# TAP test: falsey force-rgb token keeps animation decode on animation path.

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
out_default="${ARTIFACT_LOCAL_DIR}/webp-anim-lossy-default-falsey-force-rgb.six"
out_falsey="${ARTIFACT_LOCAL_DIR}/webp-anim-lossy-falsey-force-rgb.six"

msg_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >"${out_default}") || {
    echo "not ok" 1 - "default libwebp animation decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_default}" >&2
    exit 0
}

msg_falsey=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=n \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >"${out_falsey}") || {
    echo "not ok" 1 - "falsey-force-rgb libwebp animation decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_falsey}" >&2
    exit 0
}

case "${msg_falsey}" in
    *"animation background source=none"*"finalize frame_no=0 "*"finalize frame_no=1 "*)
        ;;
    *)
        echo "not ok" 1 - "falsey-force-rgb animation trace missing expected animation markers"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_falsey}" >&2
        exit 0
        ;;
esac

case "${msg_falsey}" in
    *"static decode path="*)
        echo "not ok" 1 - "falsey-force-rgb animation trace unexpectedly reported static decode path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_falsey}" >&2
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

cmp -s "${out_default}" "${out_falsey}" || {
    echo "not ok" 1 - "falsey-force-rgb unexpectedly changed libwebp animation output"
    exit 0
}

echo "ok" 1 - "falsey force-rgb keeps animation trace/output behavior unchanged"
exit 0
