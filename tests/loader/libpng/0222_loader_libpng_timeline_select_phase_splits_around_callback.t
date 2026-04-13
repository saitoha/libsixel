#!/bin/sh
# TAP test: loader/select phases split around callback handoff windows.

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
log_file="${ARTIFACT_LOCAL_DIR}/timeline-libpng-select-callback-split.json"

SIXEL_LOG_PATH="${log_file}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${input_png}" >/dev/null || {
    echo "not ok 1 - libpng conversion failed while capturing select phases"
    exit 0
}

awk '
/"worker":"loader\/manager"/ && /"role":"loader\/select"/ {
    if ($0 ~ /"event":"start"/) manager_start += 1
    if ($0 ~ /"event":"(finish|fail)"/) manager_finish += 1
}
/"worker":"loader\/libpng"/ && /"role":"loader\/select"/ {
    if ($0 ~ /"event":"start"/) candidate_start += 1
    if ($0 ~ /"event":"(finish|fail)"/) candidate_finish += 1
}
END {
    exit !(manager_start >= 2 &&
           manager_finish >= 2 &&
           candidate_start >= 2 &&
           candidate_finish >= 2)
}
' "${log_file}" || {
    echo "not ok 1 - loader/select phases did not split around callback handoff"
    exit 0
}

echo "ok 1 - loader/select phases split around callback handoff"
exit 0
