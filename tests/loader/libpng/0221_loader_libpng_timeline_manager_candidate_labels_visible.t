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

mkdir -p "${ARTIFACT_LOCAL_DIR}" || {
    echo "not ok 1 - failed to prepare ARTIFACT_LOCAL_DIR"
    exit 0
}

input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
log_file="${ARTIFACT_LOCAL_DIR}/timeline-libpng-manager-candidate.json"

SIXEL_LOG_PATH="${log_file}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${input_png}" >/dev/null || {
    echo "not ok 1 - libpng conversion failed while capturing manager labels"
    exit 0
}

mcs=0
mcf=0
mss=0
msf=0
css=0
csf=0
while IFS= read -r line; do
    case "${line}" in
        *'"worker":"loader/manager"'*'"role":"chunk/create"'*'"event":"start"'*)
            mcs=1
            ;;
        *'"worker":"loader/manager"'*'"role":"chunk/create"'*'"event":"finish"'*)
            mcf=1
            ;;
        *'"worker":"loader/manager"'*'"role":"loader/select"'*'"event":"start"'*)
            mss=1
            ;;
        *'"worker":"loader/manager"'*'"role":"loader/select"'*'"event":"finish"'*|\
        *'"worker":"loader/manager"'*'"role":"loader/select"'*'"event":"fail"'*)
            msf=1
            ;;
        *'"worker":"loader/libpng"'*'"role":"loader/select"'*'"event":"start"'*)
            css=1
            ;;
        *'"worker":"loader/libpng"'*'"role":"loader/select"'*'"event":"finish"'*|\
        *'"worker":"loader/libpng"'*'"role":"loader/select"'*'"event":"fail"'*)
            csf=1
            ;;
    esac
    test "${mcs}${mcf}${mss}${msf}${css}${csf}" = "111111" && break
done < "${log_file}"

test "${mcs}${mcf}${mss}${msf}${css}${csf}" = "111111" || {
    echo "not ok 1 - manager/candidate labels were not fully visible"
    exit 0
}

echo "ok 1 - manager and candidate labels are visible in timeline log"
exit 0
