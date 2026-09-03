#!/bin/sh
# Verify common loader thumbnail_size through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|thumbnail_size
# Registry binding: thumbnail_size_hint
# Environment range: clamp-maximum

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 || {
    printf "1..0 # SKIP gnome-thumbnailer loader is unavailable on this platform\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0026"
bin_dir="${template_root}/bin"
short_output="${artifact_dir}/0118-loader-thumbnail-size-short-$$.six"
env_output="${artifact_dir}/0118-loader-thumbnail-size-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=32" \
    "-Lgnome-thumbnailer:Z64!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "thumbnail_size short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=thumbnail_size|stored=1|binding=thumbnail_size_hint|value=64*}" != "${short_trace}" || {
    echo "not ok" 1 - "thumbnail_size short value was not stored"
    exit 0
}

test "${short_trace#*LSXTHM1|size=64|runtime_hint=0|explicit=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "thumbnail_size short value was not effective"
    exit 0
}

test "${short_trace#*LSXTHM2|size=64*}" != "${short_trace}" || {
    echo "not ok" 1 - "thumbnail_size short value did not reach thumbnailer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=64" \
    "-Lgnome-thumbnailer!" "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "thumbnail_size environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=thumbnail_size|stored=1|binding=thumbnail_size_hint|value=64*}" != "${env_trace}" || {
    echo "not ok" 1 - "thumbnail_size environment value was not stored"
    exit 0
}

test "${env_trace#*LSXTHM1|size=64|runtime_hint=0|explicit=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "thumbnail_size environment value was not effective"
    exit 0
}

test "${env_trace#*LSXTHM2|size=64*}" != "${env_trace}" || {
    echo "not ok" 1 - "thumbnail_size environment value did not reach thumbnailer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "thumbnail_size short and environment output differ"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=2147483648" \
    "-Lbuiltin!" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "thumbnail_size range conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1|*key=thumbnail_size|stored=1|binding=thumbnail_size_hint|value=2147483647*}" != "${range_trace}" || {
    echo "not ok" 1 - "thumbnail_size maximum was not clamped"
    exit 0
}

test "${range_trace#*LSXTHM1|size=2147483647|runtime_hint=0|explicit=0*}" != "${range_trace}" || {
    echo "not ok" 1 - "thumbnail_size clamped value was not effective"
    exit 0
}

zero_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=0" "-Lbuiltin!" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "thumbnail_size zero conversion failed"
    exit 0
}

test "${zero_trace#*LSXSUB1|*key=thumbnail_size|stored=1*}" = "${zero_trace}" || {
    echo "not ok" 1 - "thumbnail_size accepted zero"
    exit 0
}

test "${zero_trace#*LSXTHM1|size=512|runtime_hint=0|explicit=0*}" != "${zero_trace}" || {
    echo "not ok" 1 - "thumbnail_size zero did not retain the default"
    exit 0
}

runtime_trace=$(set +xv; SIXEL_TRACE_TOPIC=loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -w 8 \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=64" "-Lbuiltin!" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "thumbnail_size runtime hint conversion failed"
    exit 0
}

test "${runtime_trace#*LSXTHM1|size=16|runtime_hint=16|explicit=0*}" != "${runtime_trace}" || {
    echo "not ok" 1 - "resize hint did not override the environment"
    exit 0
}

precedence_trace=$(set +xv; SIXEL_TRACE_TOPIC=loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -w 8 \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=32" \
    "-Lbuiltin:Z64!" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "thumbnail_size precedence conversion failed"
    exit 0
}

test "${precedence_trace#*LSXTHM1|size=64|runtime_hint=16|explicit=1*}" != "${precedence_trace}" || {
    echo "not ok" 1 - "thumbnail_size did not override the resize hint"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "thumbnail_size image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "thumbnail_size preserves image output"
exit 0
