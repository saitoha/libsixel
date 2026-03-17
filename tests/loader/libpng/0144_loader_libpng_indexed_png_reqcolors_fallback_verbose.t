#!/bin/sh
# Verify indexed PNG near reqcolors threshold falls back to RGB by checking
# verbose planner output for libpng loader.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

reqcolors=253
image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-pal8.png"

planner_log=$(
    set +xv
    run_img2sixel --env SIXEL_THREADS=1 -v \
                  -Llibpng! -p"${reqcolors}" \
                  "${image_path}" 2>&1 >/dev/null
) || {
    echo "not ok" 1 - "libpng indexed png reqcolors fallback failed"
    exit 0
}

case "${planner_log}" in
    *"formats: source=rgb888 work=rgb888"*)
        ;;
    *)
        echo "not ok" 1 - "libpng indexed png did not take rgb fallback path"
        exit 0
        ;;
esac

echo "ok" 1 - "libpng indexed png reqcolors uses rgb fallback path"
exit 0
