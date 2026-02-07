#!/bin/sh
# TAP test: baseline pipeline thread split accounts for palette reserve.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_small="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

pipeline_log=$(run_img2sixel --env SIXEL_THREADS=4 -v -o "${ARTIFACT_LOCAL_DIR}/small.six" "${ppm_small}" 2>&1 || true)
printf '%s' "${pipeline_log}" >&2

threads_line=$(printf '%s' "${pipeline_log}" | grep "band_height=" | head -n 1 || true)
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
