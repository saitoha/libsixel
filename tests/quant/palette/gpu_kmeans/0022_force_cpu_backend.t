#!/bin/sh
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo "1..0 # SKIP Metal device is unavailable"
    exit 0
}
echo "1..1"
set -v

test "$probe_status" -eq 0 || {
    echo "not ok 1 - Metal device probe failed"
    exit 0
}

# FORCE application requires byte RGB; CMS must not promote this fixture.
trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision=8bit -Lbuiltin:cms_engine=none! -Qkmeans:backend=cpu:feedback=1:seed=1 -Gforce --lookup-policy=none -dnone \
    -Fnone -p 16 -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || {
    echo "not ok 1 - force cpu backend conversion"
    exit 0
}
case "$trace" in
    *LSXKMEANS1\|requested=cpu\|effective=cpu\|*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - force cpu backend backend selection"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - force cpu backend"
exit 0
