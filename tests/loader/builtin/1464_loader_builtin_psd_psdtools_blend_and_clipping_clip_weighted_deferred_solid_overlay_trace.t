#!/bin/sh
# Verify deferred solid-overlay diagnostic codes remain mapped in PSD source.

set -eux

source_file="${TOP_SRCDIR}/src/frompsd-trace.c"
test -f "${source_file}" || source_file="${TOP_SRCDIR}/src/frompsd.c"
test -f "${source_file}" || {
    printf "1..0 # SKIP PSD source for trace mapping is unavailable\n"
    exit 0
}

echo "1..1"
set -v

found_overlay_code=0
found_split_code=0

set +xv
while IFS= read -r source_line || test -n "${source_line}"; do
    case "${source_line}" in
        *FX_DEFERRED_SOLID_OVERLAY_CLIP*)
            found_overlay_code=1
            ;;
        *) ;;
    esac
    case "${source_line}" in
        *FX_DEFERRED_SOLID_CLIP_SPLIT*)
            found_split_code=1
            ;;
        *) ;;
    esac
done < "${source_file}"

test "${found_overlay_code}" -eq 1 || {
    echo "not ok" 1 - "PSD source is missing FX_DEFERRED_SOLID_OVERLAY_CLIP mapping"
    exit 0
}

test "${found_split_code}" -eq 1 || {
    echo "not ok" 1 - "PSD source is missing FX_DEFERRED_SOLID_CLIP_SPLIT mapping"
    exit 0
}

echo "ok" 1 - "PSD source keeps deferred solid-overlay diagnostic mappings"
exit 0
