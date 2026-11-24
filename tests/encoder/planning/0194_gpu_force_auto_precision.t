#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP requires img2sixel'
    exit 0
}
probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/resident-available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo '1..0 # SKIP requires Metal'
    exit 0
}
echo '1..1'
set -v
test "$probe_status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - Metal availability probe failed'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -G force -p 4 -o /dev/null \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-metadata-gamma-only-8-2.png" 2>&1) || {
    printf '\n%s\n' 'not ok 1 - forced Metal encode with automatic precision failed'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *'LSXDTH1|rc=0|diffuse=atkinson|'*'|diffuse_on_gpu=1|'*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - forced default diffusion did not run on Metal'
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' 'ok 1 - forced Metal encode resolves automatic precision to a GPU path'
exit 0
