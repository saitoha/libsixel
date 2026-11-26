#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Count rebuilds when switching profiles with identical matrices and TRCs.
set -eux
echo '1..1'
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/working-space/0180_icc_work_fhedt_reuse \
    "icc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    "icc:${TOP_SRCDIR}/tests/data/color-numeric/minimal-display-p3.icc" equivalent 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - equivalent ICC profiles preserve the frame color'
    exit 0
}
apply='LSXLUT2|phase=palette-apply|selected=lookup/fhedt.float32|'
build='LSXFHD1|precision=float32|'
case "$trace" in
    *"$build"*"$build"*"$build"*|*"$apply"*"$apply"*"$apply"*"$apply"*)
        false ;;
    *'icc-fhedt-frame 0'*"$apply"*"$build"*'icc-fhedt-frame 1'*"$apply"*'icc-fhedt-frame 2'*"$apply"*"$build"*)
        true ;;
    *) false ;;
esac || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - equivalent ICC profile switch must rebuild the grid'
    exit 0
}
printf '\n%s\n' 'ok 1 - equivalent ICC profile switch rebuilds the grid'
exit 0
