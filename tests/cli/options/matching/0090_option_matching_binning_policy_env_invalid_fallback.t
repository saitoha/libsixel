#!/bin/sh
# Verify an invalid binning-policy environment value keeps AUTO defaults.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env SIXEL_BINNING_POLICY=invalid \
    -Qkmeans -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - invalid binning-policy environment encode failed"
    exit 0
}

test "${trace#*LSXBIN1|requested=auto|origin=default*effective=hard*}" \
    != "${trace}" || {
    echo "not ok 1 - invalid binning-policy environment changed AUTO"
    exit 0
}

echo "ok 1 - invalid binning-policy environment keeps AUTO defaults"
exit 0
