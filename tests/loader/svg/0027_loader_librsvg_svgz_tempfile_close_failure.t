#!/bin/sh
# TAP test confirming stdin .svgz tempfile close failure reports diagnostics.

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
        --env SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1 \
        --env _SIXEL_TEST_LIBRSVG_FAIL_TEMP_SVGZ_CLOSE=1 \
        -L librsvg! - -o/dev/null 2>&1 \
        <"${svgz_path}"
) || status="$?"

test "${status}" -ne 0 || {
    printf '\n%s\n' "not ok 1 - stdin .svgz tempfile close failpoint unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"failed to close temporary .svgz file."*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - stdin .svgz tempfile close failure diagnostics missing"
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - stdin .svgz tempfile close failure diagnostics are reported"
exit 0
