#!/bin/sh
# TAP test confirming file-path .svgz decode emits trace decode_mode=file.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

svgz_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svgz"
trace_log=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=loader \
        -L librsvg! "${svgz_path}" -o/dev/null 2>&1
) || {
    printf '\n%s\n' "not ok 1 - trace-enabled file-path .svgz conversion failed"
    exit 0
}

case "${trace_log}" in
    *"librsvg: decode_mode=file"*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - file-path .svgz trace mode was not reported"
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - librsvg .svgz file-path trace decode mode is reported"
exit 0
