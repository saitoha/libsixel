#!/bin/sh
# TAP test: forced GPU palette apply leaves post-palette workers for encode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 16 --lookup-policy=none -o /dev/null "${probe_image}" || {
    printf "1..0 # SKIP forced GPU palette apply is unavailable\n"
    exit 0
}

echo "1..1"
set -v

pipeline_log=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
                  -G force -d none -p 16 --lookup-policy=none \
                  -v -o /dev/null "${probe_image}" 2>&1) || {
    echo "not ok" 1 - "gpu force thread split run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

case "${pipeline_log}" in
    *"threads: dither=0 encode="*) ;;
    *)
        echo "not ok" 1 - "gpu force keeps encode worker budget"
        exit 0
        ;;
esac

echo "ok" 1 - "gpu force keeps encode worker budget"
exit 0
