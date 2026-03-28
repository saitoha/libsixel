#!/bin/sh
# TAP test confirming stdin .svgz reject path emits trace decode_mode=stdin_svgz_rejected.

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
status=0
trace_log=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=loader \
        -L librsvg! - -o/dev/null 2>&1 \
        <"${svgz_path}"
) || status="$?"

test "${status}" -ne 0 || {
    echo "not ok" 1 - "trace-enabled stdin .svgz reject path unexpectedly succeeded"
    exit 0
}

case "${trace_log}" in
    *"librsvg: decode_mode=stdin_svgz_rejected"*)
        ;;
    *)
        echo "not ok" 1 - "stdin .svgz reject trace mode was not reported"
        exit 0
        ;;
esac

echo "ok" 1 - "librsvg stdin .svgz reject trace decode mode is reported"
exit 0
