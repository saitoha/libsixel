#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Share one FHEDT grid for two builtin GIF frames with a fixed PNG palette.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-0}:${SIXEL_ENABLE_THREADS-0}" = 1:1 || {
    printf '\n%s\n' 'ok 1 # SKIP requires img2sixel and threads'
    exit 0
}
parallel_dither=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/allocation/0001_allocation_slots parallel-dither) || {
    printf '\n%s\n' 'not ok 1 - cannot query parallel dither capability'
    exit 0
}
test "$parallel_dither" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP compiler disables parallel dither bands'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -7 -g -ldisable -Goff -Lbuiltin! -Xauto \
    --threading-policy=greedy:B4:dither=2:encode=2 \
    -dfs:scan=raster:band_width=6:band_overwrap=0 -~fhedt:R64:shared=1 \
    "-Wicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    -m "${TOP_SRCDIR}/tests/data/color-numeric/icc-cluster-alpha.png" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose2.gif" \
    2>&1 >/dev/null) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - parallel ICC fixed-palette encode'
    exit 0
}
case "$trace" in
    *'LSXDTH1|rc=0|diffuse=fs|scan=raster|'*'|apply=2|apply_ok=2|'*'|band_height=6|band_overlap=0|'*'|dither_threads=2|'*'|allocation_mode=pipeline|'*) true ;;
    *) false ;;
esac || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - expected two parallel FS applications'
    exit 0
}
build='LSXFHD1|precision=float32|'
case "$trace" in
    *"$build"*"$build"*) false ;;
    *"$build"*) true ;;
    *) false ;;
esac || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - expected one FHEDT build for both frames'
    exit 0
}
printf '\n%s\n' 'ok 1 - parallel ICC FS shares one FHEDT grid across both frames'
exit 0
