#!/bin/sh
# TAP test: libpng timeline exposes required loader phases.

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
log_file="${ARTIFACT_LOCAL_DIR}/timeline-libpng-required-phases.json"

SIXEL_LOG_PATH="${log_file}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${input_png}" >/dev/null || {
    echo "not ok 1 - libpng conversion failed while capturing timeline"
    exit 0
}

hs=0
hf=0
ds=0
df=0
es=0
ef=0
while IFS= read -r line; do
    case "${line}" in
        *'"worker":"loader/libpng"'*'"role":"header/read"'*'"event":"start"'*)
            hs=1
            ;;
        *'"worker":"loader/libpng"'*'"role":"header/read"'*'"event":"finish"'*)
            hf=1
            ;;
        *'"worker":"loader/libpng"'*'"role":"decode/pixels"'*'"event":"start"'*)
            ds=1
            ;;
        *'"worker":"loader/libpng"'*'"role":"decode/pixels"'*'"event":"finish"'*)
            df=1
            ;;
        *'"worker":"loader/libpng"'*'"role":"emit/frame"'*'"event":"start"'*)
            es=1
            ;;
        *'"worker":"loader/libpng"'*'"role":"emit/frame"'*'"event":"finish"'*)
            ef=1
            ;;
    esac
    test "${hs}${hf}${ds}${df}${es}${ef}" = "111111" && break
done < "${log_file}"

test "${hs}${hf}${ds}${df}${es}${ef}" = "111111" || {
    echo "not ok 1 - required loader phases were not fully visible"
    exit 0
}

echo "ok 1 - libpng timeline exposes required phases"
exit 0
