#!/bin/sh
# Verify deferred miter stroke-adjust diagnostics remain mapped in frompsd.c.

set -eux

test -f "${TOP_SRCDIR}/src/frompsd.c" || {
    printf "1..0 # SKIP src/frompsd.c is unavailable\n"
    exit 0
}

echo "1..1"
set -v
set +xv

code_block=''
miter_block=''
status=0

code_block=$(sed -n '430,490p' "${TOP_SRCDIR}/src/frompsd.c" 2>/dev/null) || \
    status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load stroke-adjust code map from frompsd.c"
    exit 0
}

test "${code_block#*FX_STROKE_ADJUST_DEFER*}" != "${code_block}" || {
    echo "not ok" 1 - "frompsd.c is missing FX_STROKE_ADJUST_DEFER mapping"
    exit 0
}

miter_block=$(sed -n '24720,24820p' "${TOP_SRCDIR}/src/frompsd.c" 2>/dev/null) || \
    status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load deferred miter branch from frompsd.c"
    exit 0
}

test "${miter_block#*SIXEL_BUILTIN_PSD_STROKE_JOIN_MITER*}" != "${miter_block}" || {
    echo "not ok" 1 - "frompsd.c lost deferred miter join branch"
    exit 0
}

test "${miter_block#*vector_stroke_adjust != 0*}" != "${miter_block}" || {
    echo "not ok" 1 - "frompsd.c lost deferred stroke-adjust gate"
    exit 0
}

test "${miter_block#*sixel_builtin_psd_compute_stroke_coverage_join*}" != "${miter_block}" || {
    echo "not ok" 1 - "frompsd.c lost deferred join coverage call"
    exit 0
}

echo "ok" 1 - "frompsd.c keeps deferred miter stroke-adjust diagnostic mapping"
exit 0
