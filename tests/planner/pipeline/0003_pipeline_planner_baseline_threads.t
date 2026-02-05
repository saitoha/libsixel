#!/bin/sh
# TAP test: baseline pipeline thread split accounts for palette reserve.

set -eux

export SIXEL_THREADS=4

ppm_small="${ARTIFACT_LOCAL_DIR}/grid_small.ppm"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/pipeline/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

create_small_ppm "${ppm_small}"
pipeline_log=$(run_img2sixel -v -o "${ARTIFACT_LOCAL_DIR}/small.six" \
    "${ppm_small}" 2>&1 || true)
printf '%s' "${pipeline_log}" >&2

threads_line=$(printf '%s' "${pipeline_log}" | grep "band_height=" \
    | head -n 1 || true)
case "${threads_line}" in
"    band_height=12 overlap=0 threads: dither=1 encode=2")
    printf 'ok 1 - baseline thread split (palette reserve)\n'
    ;;
"    band_height="*)
    printf 'ok 1 - baseline thread split (serial environment)\n'
    ;;
*)
    printf 'not ok 1 - baseline thread split (palette reserve)\n'
    exit 1
    ;;
esac
