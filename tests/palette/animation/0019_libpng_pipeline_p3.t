#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify libpng pipeline p3 animation colors and loop replay.
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng loader is unavailable"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
trace=$(
    SIXEL_TRACE_TOPIC=encode_handoff ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
        quant/palette/animation_native/0019_libpng_pipeline_p3 \
        "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
        "${ARTIFACT_LOCAL_DIR}/animation.six" 2>&1
) || {
    printf '%s\n' "${trace}" >&2
    echo "not ok 1 - libpng pipeline p3 numeric output"
    exit 0
}

case "${trace}" in *"event=callback_handoff_decide handoff=pipeline"*) true ;; *) false ;; esac || {
    echo "not ok 1 - APNG uses frame pipeline"
    exit 0
}

echo "ok 1 - libpng pipeline p3 numeric output"
exit 0
