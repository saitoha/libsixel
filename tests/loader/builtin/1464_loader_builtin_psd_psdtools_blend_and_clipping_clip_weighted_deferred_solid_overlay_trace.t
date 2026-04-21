#!/bin/sh
# Verify deferred solid-overlay diagnostic codes remain mapped in frompsd.c.

set -eux

test -f "${TOP_SRCDIR}/src/frompsd.c" || {
    printf "1..0 # SKIP src/frompsd.c is unavailable\n"
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
done < "${TOP_SRCDIR}/src/frompsd.c"

test "${found_overlay_code}" -eq 1 || {
    echo "not ok" 1 - "frompsd.c is missing FX_DEFERRED_SOLID_OVERLAY_CLIP mapping"
    exit 0
}

test "${found_split_code}" -eq 1 || {
    echo "not ok" 1 - "frompsd.c is missing FX_DEFERRED_SOLID_CLIP_SPLIT mapping"
    exit 0
}

echo "ok" 1 - "frompsd.c keeps deferred solid-overlay diagnostic mappings"
exit 0
