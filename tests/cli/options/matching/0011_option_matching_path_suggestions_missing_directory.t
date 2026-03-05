#!/bin/sh
# TAP test verifying missing directories are reported with path suggestions on.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              -m "${ARTIFACT_LOCAL_DIR}/not-there/map.gpl" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
              -o/dev/null 2>&1) && {
    echo "not ok" 1 "missing directory unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'Directory "'*)
        ;;
    *)
        echo "not ok" 1 "missing directory diagnostics were not emitted"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *'does not exist.'*)
        ;;
    *)
        echo "not ok" 1 "missing directory path reports unsupported suggestion lookup"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 "missing directory diagnostic is emitted"
exit 0
