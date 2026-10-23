#!/bin/sh
# Policy: docs/threading/encoder.md
# Compare prepare-ahead animation playback with serial frame preparation.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_motion_64_2frame.gif"
serial_delta="${ARTIFACT_LOCAL_DIR}/0019-prepare-ahead-serial-delta-$$.six"
ahead_delta="${ARTIFACT_LOCAL_DIR}/0019-prepare-ahead-ahead-delta-$$.six"
serial_full="${ARTIFACT_LOCAL_DIR}/0019-prepare-ahead-serial-full-$$.six"
ahead_full="${ARTIFACT_LOCAL_DIR}/0019-prepare-ahead-ahead-full-$$.six"

# The second frame is prepared while the first is emitted. 6delta makes its
# palette depend on the keep key that the first frame's history decides; a
# small palette makes that one reserved entry change the output.
ahead_trace=$(set +xv; SIXEL_TRACE_TOPIC=encode_handoff \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -g -l disable -=4 -d fs -p16 \
    -Z delta -o "${ahead_delta}" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - prepare-ahead delta playback failed"
    exit 0
}
case "${ahead_trace}" in *event=callback_handoff_decide\ handoff=pipeline*) true ;; *) false ;; esac || {
    echo "not ok 1 - animation playback did not select the frame pipeline"
    exit 0
}
_SIXEL_PREPARE_AHEAD=0 ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -g -l disable \
    -=4 -d fs -p16 -Z delta -o "${serial_delta}" "${input_image}" || {
    echo "not ok 1 - serial delta playback failed"
    exit 0
}
cmp -s "${serial_delta}" "${ahead_delta}" || {
    echo "not ok 1 - prepare-ahead delta playback changed the output"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -g -l disable -=4 -d fs -p16 \
    -o "${ahead_full}" "${input_image}" || {
    echo "not ok 1 - prepare-ahead full playback failed"
    exit 0
}
_SIXEL_PREPARE_AHEAD=0 ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -g -l disable \
    -=4 -d fs -p16 -o "${serial_full}" "${input_image}" || {
    echo "not ok 1 - serial full playback failed"
    exit 0
}
cmp -s "${serial_full}" "${ahead_full}" || {
    echo "not ok 1 - prepare-ahead full playback changed the output"
    exit 0
}
echo "ok 1 - prepare-ahead playback preserves serial output"
exit 0
