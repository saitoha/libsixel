#!/bin/sh
# Verify deferred miter stroke-adjust diagnostics stay mapped after split.

set -eux

trace_source="${TOP_SRCDIR}/src/frompsd-trace.c"
impl_source="${TOP_SRCDIR}/src/frompsd.c"
test -f "${trace_source}" || trace_source="${impl_source}"
test -f "${trace_source}" || {
    printf "1..0 # SKIP PSD trace source is unavailable\n"
    exit 0
}
test -f "${impl_source}" || {
    printf "1..0 # SKIP src/frompsd.c is unavailable\n"
    exit 0
}

echo "1..1"
set -v
set +xv

found_code=0
found_miter_branch=0
found_adjust_gate=0
found_join_call=0
in_miter_branch=0

while IFS= read -r source_line || test -n "${source_line}"; do
    case "${source_line}" in
        *FX_STROKE_ADJUST_DEFER*)
            found_code=1
            ;;
        *) ;;
    esac
done < "${trace_source}"

while IFS= read -r source_line || test -n "${source_line}"; do
    case "${source_line}" in
        *SIXEL_BUILTIN_PSD_STROKE_JOIN_MITER*)
            in_miter_branch=1
            found_miter_branch=1
            ;;
        *) ;;
    esac
    test "${in_miter_branch}" -eq 1 || continue
    case "${source_line}" in
        *vector_stroke_adjust\ \!=\ 0*)
            found_adjust_gate=1
            ;;
        *) ;;
    esac
    case "${source_line}" in
        *sixel_builtin_psd_compute_stroke_coverage_join*)
            found_join_call=1
            ;;
        *) ;;
    esac
done < "${impl_source}"

test "${found_code}" -eq 1 || {
    echo "not ok" 1 - "PSD trace source is missing FX_STROKE_ADJUST_DEFER mapping"
    exit 0
}

test "${found_miter_branch}" -eq 1 || {
    echo "not ok" 1 - "frompsd.c lost deferred miter join branch"
    exit 0
}

test "${found_adjust_gate}" -eq 1 || {
    echo "not ok" 1 - "frompsd.c lost deferred stroke-adjust gate"
    exit 0
}

test "${found_join_call}" -eq 1 || {
    echo "not ok" 1 - "frompsd.c lost deferred join coverage call"
    exit 0
}

echo "ok" 1 - "frompsd.c keeps deferred miter stroke-adjust diagnostic mapping"
exit 0
