#!/bin/sh
# Verify libwebp:max_output_frames preserves bounded animation output.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP|max_output_frames
# Registry binding: libwebp_max_output_frames
# Environment range: clamp-maximum

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

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"
reference_sixel="${TOP_SRCDIR}/tests/data/inputs/formats/animated_lossless_8x8_2frame_min_loop_disable_reference.six"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/animated-8x8-frame1-reference.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0107-loader-libwebp-max-frames-short-$$.six"
env_output="${artifact_dir}/0107-loader-libwebp-max-frames-env-$$.six"
quality_output="${artifact_dir}/0107-loader-libwebp-max-frames-quality-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibwebp:M2!" -=1 -ldisable "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "libwebp:max_output_frames short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=max_output_frames|stored=1|binding=libwebp_max_output_frames|value=2*}" != "${short_trace}" || {
    echo "not ok" 1 - "max_output_frames short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=2" \
    "-Llibwebp!" -=1 -ldisable "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "libwebp:max_output_frames env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=max_output_frames|stored=1|binding=libwebp_max_output_frames|value=2*}" != "${env_trace}" || {
    echo "not ok" 1 - "max_output_frames environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=262145" \
    "-Llibwebp!" -=1 -ldisable "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "max_output_frames upper conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1|*key=max_output_frames|stored=1|binding=libwebp_max_output_frames|value=262144*}" != "${range_trace}" || {
    echo "not ok" 1 - "max_output_frames upper endpoint was not clamped"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibwebp:M1!" -=1 -ldisable "${input_image}" >/dev/null 2>&1 && {
    echo "not ok" 1 - "max_output_frames=1 did not stop the second frame"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "max_output_frames short and env output differ"
    exit 0
}

cmp -s "${reference_sixel}" "${short_output}" || {
    echo "not ok" 1 - "max_output_frames animation output regressed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibwebp:M2!" -=1 -S "${input_image}" >"${quality_output}" || {
    echo "not ok" 1 - "max_output_frames static quality conversion failed"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${quality_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "max_output_frames image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "libwebp:max_output_frames preserves animation output"
exit 0
