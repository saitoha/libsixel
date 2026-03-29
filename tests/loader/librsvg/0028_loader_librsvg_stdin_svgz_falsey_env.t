#!/bin/sh
# TAP test confirming falsey stdin .svgz opt-in keeps default rejection.

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
msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=no \
        -L librsvg! - -o/dev/null 2>&1 \
        <"${svgz_path}"
) || status="$?"

test "${status}" -ne 0 || {
    echo "not ok" 1 - "falsey stdin .svgz opt-in unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"requires file-path decode"*)
        ;;
    *)
        echo "not ok" 1 - "falsey stdin .svgz opt-in missing rejection diagnostics"
        exit 0
        ;;
esac

echo "ok" 1 - "falsey stdin .svgz opt-in keeps rejection behavior"
exit 0
