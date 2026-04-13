#!/bin/sh
# TAP test: loader manager/candidate timeline labels are visible for libpng.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}

test -n "${ARTIFACT_LOCAL_DIR-}" || {
    printf "1..0 # SKIP ARTIFACT_LOCAL_DIR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
log_file="${ARTIFACT_LOCAL_DIR}/timeline-libpng-manager-candidate.json"

SIXEL_LOG_PATH="${log_file}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${input_png}" >/dev/null || {
    echo "not ok 1 - libpng conversion failed while capturing manager labels"
    exit 0
}

awk '
/"worker":"loader\/manager"/ {
    if ($0 ~ /"role":"chunk\/create"/ && $0 ~ /"event":"start"/) mcs = 1
    if ($0 ~ /"role":"chunk\/create"/ && $0 ~ /"event":"finish"/) mcf = 1
    if ($0 ~ /"role":"loader\/select"/ && $0 ~ /"event":"start"/) mss = 1
    if ($0 ~ /"role":"loader\/select"/ &&
        $0 ~ /"event":"(finish|fail)"/) msf = 1
}
/"worker":"loader\/libpng"/ {
    if ($0 ~ /"role":"loader\/select"/ && $0 ~ /"event":"start"/) css = 1
    if ($0 ~ /"role":"loader\/select"/ &&
        $0 ~ /"event":"(finish|fail)"/) csf = 1
}
END {
    exit !(mcs && mcf && mss && msf && css && csf)
}
' "${log_file}" || {
    echo "not ok 1 - manager/candidate labels were not fully visible"
    exit 0
}

echo "ok 1 - manager and candidate labels are visible in timeline log"
exit 0
