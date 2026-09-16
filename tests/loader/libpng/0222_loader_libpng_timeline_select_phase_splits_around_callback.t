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

mkdir -p "${ARTIFACT_LOCAL_DIR}" || {
    echo "not ok 1 - failed to prepare ARTIFACT_LOCAL_DIR"
    exit 0
}

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"
log_file="${ARTIFACT_LOCAL_DIR}/timeline-libpng-select-callback-split.json"

SIXEL_LOG_PATH="${log_file}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${input_png}" >/dev/null || {
    echo "not ok 1 - libpng conversion failed while capturing select phases"
    exit 0
}

manager_start=0
manager_finish=0
candidate_start=0
candidate_finish=0
while IFS= read -r line; do
    case "${line}" in
        *'"worker":"loader/manager"'*'"role":"loader/select"'*'"event":"start"'*)
            manager_start=$((manager_start + 1))
            ;;
        *'"worker":"loader/manager"'*'"role":"loader/select"'*'"event":"finish"'*|\
        *'"worker":"loader/manager"'*'"role":"loader/select"'*'"event":"fail"'*)
            manager_finish=$((manager_finish + 1))
            ;;
        *'"worker":"loader/libpng"'*'"role":"loader/select"'*'"event":"start"'*)
            candidate_start=$((candidate_start + 1))
            ;;
        *'"worker":"loader/libpng"'*'"role":"loader/select"'*'"event":"finish"'*|\
        *'"worker":"loader/libpng"'*'"role":"loader/select"'*'"event":"fail"'*)
            candidate_finish=$((candidate_finish + 1))
            ;;
    esac
    test "${manager_start}" -ge 2 || continue
    test "${manager_finish}" -ge 2 || continue
    test "${candidate_start}" -ge 2 || continue
    test "${candidate_finish}" -ge 2 || continue
    break
done < "${log_file}"

test "${manager_start}" -ge 2 || {
    echo "not ok 1 - loader/select phases did not split around callback handoff"
    exit 0
}
test "${manager_finish}" -ge 2 || {
    echo "not ok 1 - loader/select phases did not split around callback handoff"
    exit 0
}
test "${candidate_start}" -ge 2 || {
    echo "not ok 1 - loader/select phases did not split around callback handoff"
    exit 0
}
test "${candidate_finish}" -ge 2 || {
    echo "not ok 1 - loader/select phases did not split around callback handoff"
    exit 0
}

echo "ok 1 - loader/select phases split around callback handoff"
exit 0
