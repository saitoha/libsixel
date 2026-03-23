#!/bin/sh
# TAP test verifying libwebp -L suboptions are accepted.

set -euxv

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"

probe_output=$(
    set +xv
    run_img2sixel -Llibwebp:cms=0! \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" \
        -o/dev/null 2>&1
) || probe_status=$?

case "${probe_output}" in
    *"invalid argument for -L,--loaders option"*)
        echo "not ok" 1 - "-L libwebp suboptions were rejected by option parser"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${probe_output}" >&2
        exit 0
        ;;
    *)
        ;;
esac

test "${probe_status-}" = "" || {
    echo "not ok" 1 - "-L libwebp suboptions were rejected by option parser"
    exit 0
}

echo "ok" 1 - "-L accepts libwebp:cms suboptions"
exit 0
