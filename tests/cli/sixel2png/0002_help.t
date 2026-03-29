#!/bin/sh
# TAP test verifying sixel2png reports usage information.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

help_output="${ARTIFACT_LOCAL_DIR}/help.txt"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -H 1>"${help_output}" || {
    echo "not ok" 1 - "-H exited with failure"
    exit 0
}

IFS= read -r help_line < "${help_output}" || help_line=""
case "${help_line}" in
    Usage:\ sixel2png*) ;;
    *)
        echo "not ok" 1 - "help usage header missing"
        exit 0
        ;;
esac

test -n "${help_line}" || {
    echo "not ok" 1 - "help usage header missing"
    exit 0
}

echo "ok" 1 - "-H prints usage"
exit 0
