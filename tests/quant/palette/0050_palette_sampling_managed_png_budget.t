#!/bin/sh
# Policy: docs/loader/builtin/png-source-precision.md
# Verify builtin PNG float normalization consumes a color-conversion slot.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
test "${HAVE_LCMS2-}" != 1 || {
    echo "1..0 # SKIP direct builtin gAMA normalization requires no lcms2"
    exit 0
}
echo "1..1"
set -v

# The fixture's gAMA/cHRM requires float normalization with CMS enabled.
# Two workers leave no palette worker after the color-conversion slot.
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=2 \
    --cms-engine=builtin -Qheckbert -d none -p 16 "-~none" \
    -L builtin -ldisable "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - managed PNG sampling failed"
    exit 0
}
test "${trace#*LSXSPL1|requested=auto|effective=full-frame|source=preprocessed-frame|origin=auto|phase=executed|reason=resource-profile|threads=2|heavy=1|budget_async=0|job_ready=0*}" != "${trace}" || {
    echo "not ok 1 - managed PNG did not reserve color-conversion work"
    exit 0
}
test "${trace#*LSXSMP1|*}" = "${trace}" || {
    echo "not ok 1 - managed PNG ran adaptive sampling without a worker"
    exit 0
}
echo "ok 1 - managed PNG reserves its color-conversion work"
exit 0
