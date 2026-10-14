#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Explicit limits override conflicting environment values.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma \
    --env SIXEL_UPDATE_PROGRESSIVE_STOP_THRESHOLD=2 \
    --env SIXEL_UPDATE_PROGRESSIVE_MAX_PASSES=2 \
    --env SIXEL_UPDATE_PROGRESSIVE_MAX_SECONDS=1e-300 \
    -Z progressive:stop_threshold=0:max_passes=1:max_seconds=0 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - progressive conversion failed"
    exit 0
}
case "${trace}" in *"progressive: stop=max_passes passes=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - limit cli environment precedence"
    exit 0
}
echo "ok 1 - limit cli environment precedence"
exit 0
