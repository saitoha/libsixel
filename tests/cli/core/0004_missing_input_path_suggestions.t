#!/bin/sh
# TAP test ensuring img2sixel reports suggestions for missing input paths.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

missing_input="${TOP_SRCDIR}/tests/data/inputs/xxxxx.xxx"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              --env SIXEL_TRACE_TOPIC=file_open:suggestion:lifecycle \
              "${missing_input}" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "accepts missing input path"
    exit 0
}

case "${msg}" in
    *'Suggestions:'*|*'Suggestion lookup unavailable on this build.'*|*'No nearby matches were found in'*)
        ;;
    *)
        echo "not ok" 1 - "missing path suggestion diagnostics"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "missing input path suggestion diagnostics reported"
exit 0
