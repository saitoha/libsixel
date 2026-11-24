#!/bin/sh
# TAP test ensuring png: prefix writes to filesystem path.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

out_path="${ARTIFACT_LOCAL_DIR}/out.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -o "png:${out_path}" <"${TOP_SRCDIR}/images/map8.six" || {
    printf '\n%s\n' "not ok 1 - prefixed output command failed"
    exit 0
}

test -s "${out_path}" || {
    printf '\n%s\n' "not ok 1 - prefixed output missing"
    exit 0
}

printf '\n%s\n' "ok 1 - prefixed output trims scheme"
exit 0
