#!/bin/sh
# TAP test: builtin PNG default background policy stays file_first for bKGD.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
builtin_default="${ARTIFACT_LOCAL_DIR}/builtin_bgan6a08_default.six"
builtin_file_first="${ARTIFACT_LOCAL_DIR}/builtin_bgan6a08_file_first.six"
builtin_explicit_first="${ARTIFACT_LOCAL_DIR}/builtin_bgan6a08_explicit_first.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! \
              -d fs:scan=raster \
              "${input_png}" >"${builtin_default}" || {
    echo "not ok" 1 - "builtin default bKGD composite conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_BACKGROUND_POLICY=file_first \
              -Lbuiltin:cms_engine=none! -d fs:scan=raster \
              "${input_png}" >"${builtin_file_first}" || {
    echo "not ok" 1 - "builtin file_first bKGD conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_BACKGROUND_POLICY=explicit_first \
              -Lbuiltin:cms_engine=none! -d fs:scan=raster \
              -B#fff "${input_png}" >"${builtin_explicit_first}" || {
    echo "not ok" 1 - "builtin explicit_first bKGD conversion failed"
    exit 0
}

cmp -s "${builtin_default}" "${builtin_file_first}" || {
    echo "not ok" 1 - "builtin default policy mismatch against file_first"
    exit 0
}

cmp -s "${builtin_default}" "${builtin_explicit_first}" && {
    echo "not ok" 1 - "builtin explicit_first did not override embedded bKGD"
    exit 0
}

echo "ok" 1 - "builtin default bKGD policy remains file_first"
exit 0
