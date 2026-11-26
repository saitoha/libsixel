#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Check raster Floyd–Steinberg residuals across two independent ICC bands.
set -eux
echo '1..1'
set -v

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP internal threads unavailable'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/working-space/0181_icc_work_parallel_fs \
    "icc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - parallel Floyd–Steinberg uses ICC residuals'
    exit 0
}
case "$trace" in *'ICC FS: parallel bands unavailable'*)
    printf '\n%s\n' 'ok 1 # SKIP parallel dither bands unavailable'
    exit 0 ;;
esac
printf '\n%s\n' 'ok 1 - parallel Floyd–Steinberg uses ICC residuals'
exit 0
