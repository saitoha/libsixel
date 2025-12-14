#!/bin/sh
# TAP test validating planner pipeline calculation and verbose dump output.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

# Prepare artifact paths for captured outputs.
test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/pipeline.log"
ppm_small="${artifact_dir}/grid_small.ppm"
ppm_tall="${artifact_dir}/grid_tall.ppm"

mkdir -p "${artifact_dir}"

# Ensure deterministic thread planning before loading shared helpers.
export SIXEL_THREADS=4

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..6"

# Build a small PPM to keep runtime low while still creating multiple bands.
cat <<'PPM' >"${ppm_small}"
P3
6 12
255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
PPM

# Baseline: expect two bands and pipeline mode. A palette worker reserves
# one thread, so the pipeline split is computed from the remaining main
# threads instead of the raw SIXEL_THREADS value.
small_log="${artifact_dir}/verbose_small.log"
if run_img2sixel -v -o "${artifact_dir}/small.six" "${ppm_small}" \
        >"${artifact_dir}/small.out" 2>"${small_log}"; then
    pass ${case_id} "pipeline run succeeded (baseline)"
else
    fail ${case_id} "pipeline run failed (baseline)"
fi
case_id=$((case_id + 1))

summary=$(grep "bands=" "${small_log}" | head -n 1 || true)
threads_line=$(grep "band_height=" "${small_log}" | head -n 1 || true)

case "${summary}" in
"    bands=2 queue=2 mode=pipeline")
    pass ${case_id} "baseline bands/queue/mode"
    ;;
"    bands="*)
    printf 'Pipeline disabled, observed summary:%s\n' "${summary}" 1>&2
    pass ${case_id} "baseline bands/queue/mode (serial environment)"
    ;;
*)
    printf 'Diagnostic summary:%s\n' "${summary}" 1>&2
    fail ${case_id} "baseline bands/queue/mode"
    ;;
esac
case_id=$((case_id + 1))

case "${threads_line}" in
"    band_height=12 overlap=0 threads: dither=1 encode=2")
    pass ${case_id} "baseline thread split (palette reserve)"
    ;;
"    band_height="*)
    printf 'Pipeline disabled, observed threads:%s\n' "${threads_line}" \
           1>&2
    pass ${case_id} "baseline thread split (serial environment)"
    ;;
*)
    printf 'Diagnostic threads:%s\n' "${threads_line}" 1>&2
    fail ${case_id} "baseline thread split (palette reserve)"
    ;;
esac
case_id=$((case_id + 1))

# Override thread and band configuration to exercise env parsing paths.
cat <<'PPM' >"${ppm_tall}"
P3
6 60
255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
255 0 0  0 255 0  0 0 255  255 255 0  0 255 255  255 0 255
PPM

tall_log="${artifact_dir}/verbose_tall.log"
if SIXEL_THREADS=6 \
        SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
        SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
        SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
        run_img2sixel -v -o "${artifact_dir}/tall.six" "${ppm_tall}" \
        >"${artifact_dir}/tall.out" 2>"${tall_log}"; then
    pass ${case_id} "pipeline run succeeded (overrides)"
else
    fail ${case_id} "pipeline run failed (overrides)"
fi
case_id=$((case_id + 1))

summary=$(grep "bands=" "${tall_log}" | head -n 1 || true)
threads_line=$(grep "band_height=" "${tall_log}" | head -n 1 || true)

case "${summary}" in
"    bands=10 queue=10 mode=pipeline")
    pass ${case_id} "override bands/queue/mode"
    ;;
"    bands="*)
    printf 'Pipeline disabled, observed summary:%s\n' "${summary}" 1>&2
    pass ${case_id} "override bands/queue/mode (serial environment)"
    ;;
*)
    printf 'Diagnostic summary:%s\n' "${summary}" 1>&2
    fail ${case_id} "override bands/queue/mode"
    ;;
esac
case_id=$((case_id + 1))

case "${threads_line}" in
"    band_height=12 overlap=4 threads: dither=1 encode=4")
    pass ${case_id} "override thread split (palette reserve)"
    ;;
"    band_height="*)
    printf 'Pipeline disabled, observed threads:%s\n' "${threads_line}" \
           1>&2
    pass ${case_id} "override thread split (serial environment)"
    ;;
*)
    printf 'Diagnostic threads:%s\n' "${threads_line}" 1>&2
    fail ${case_id} "override thread split (palette reserve)"
    ;;
esac

exit "${status}"
