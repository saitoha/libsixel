#!/bin/sh
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# Successful backend execution proves availability. Probe only when the real
# conversion or backend assertion fails, so a capable device starts once.
conversion_status=0
backend_ok=0
probe_status=0
# FORCE application requires byte RGB; CMS must not promote this fixture.
trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! -Gforce -Qkmeans:backend=gpu:seed=1 --lookup-policy=none -dnone \
    -Fnone -p 16 -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || conversion_status=$?
case "$trace" in
    *LSXKMEANS1\|requested=gpu\|effective=metal\|*) backend_ok=1 ;;
    *) : ;;
esac
test "$conversion_status:$backend_ok" = 0:1 ||
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo "ok 1 # SKIP Metal device is unavailable"
    exit 0
}
test "$probe_status" -eq 0 || {
    echo "not ok 1 - Metal device probe failed"
    exit 0
}
test "$conversion_status" -eq 0 || {
    echo "not ok 1 - force dispatch conversion"
    exit 0
}
test "$backend_ok" -eq 1 || {
    echo "not ok 1 - force dispatch backend selection"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - force dispatch"
exit 0
