#!/bin/sh
# TAP test: APNG builtin static start-frame selection follows start-frame
# controls.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

trace_default=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=apng \
    -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
    2>&1 >"${ARTIFACT_LOCAL_DIR}/apng_builtin_update_frame0.six") || {
    echo "not ok" 1 - "APNG builtin default frame extraction failed"
    exit 0
}

trace_frame1=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=apng \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
    2>&1 >"${ARTIFACT_LOCAL_DIR}/apng_builtin_update_frame1.six") || {
    echo "not ok" 1 - "APNG builtin start-frame extraction failed"
    exit 0
}

case "${trace_default}" in
    *"source_frame=1"*)
        ;;
    *)
        echo "not ok" 1 - "APNG builtin default static decode did not emit frame 1"
        exit 0
        ;;
esac

case "${trace_frame1}" in
    *"source_frame=2"*)
        ;;
    *)
        echo "not ok" 1 - "APNG builtin start-frame=1 did not emit frame 2"
        exit 0
        ;;
esac

cmp -s "${ARTIFACT_LOCAL_DIR}/apng_builtin_update_frame0.six" \
       "${ARTIFACT_LOCAL_DIR}/apng_builtin_update_frame1.six" && {
    echo "not ok" 1 - "APNG builtin start-frame override did not change static output"
    exit 0
}

echo "ok" 1 - "APNG builtin static start-frame selection follows controls"
exit 0
