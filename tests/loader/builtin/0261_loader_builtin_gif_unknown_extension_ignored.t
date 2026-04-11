#!/bin/sh
# TAP test: legal unknown GIF extension block is ignored.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

base_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
unknown_ext_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-unknown-extension-safe.gif"
out_base="${ARTIFACT_LOCAL_DIR}/builtin_gif_unknown_ext_base.six"
out_unknown="${ARTIFACT_LOCAL_DIR}/builtin_gif_unknown_ext_unknown.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin! -S -ldisable -d none:scan=raster -p 256 \
              "${base_gif}" >"${out_base}" || {
    echo "not ok" 1 - "baseline GIF decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin! -S -ldisable -d none:scan=raster -p 256 \
              "${unknown_ext_gif}" >"${out_unknown}" || {
    echo "not ok" 1 - "unknown-extension GIF decode failed"
    exit 0
}

cmp -s "${out_base}" "${out_unknown}" || {
    echo "not ok" 1 - "unknown legal extension changed decoded output"
    exit 0
}

echo "ok" 1 - "unknown legal GIF extension is ignored"
exit 0
