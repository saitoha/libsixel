#!/bin/sh
# Verify --background-policy overrides loader and environment defaults.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png"

file_first=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite \
    --env SIXEL_BACKGROUND_POLICY=explicit_first \
    --background-policy=file_first \
    -L builtin:Pexplicit_first:cms_engine=none! \
    -B '#ffffff' -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "file-first background-policy render failed"
    exit 0
}

explicit_first=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite \
    --env SIXEL_BACKGROUND_POLICY=file_first \
    --background-policy=explicit_first \
    -L builtin:Pfile_first:cms_engine=none! \
    -B '#ffffff' -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "explicit-first background-policy render failed"
    exit 0
}

test "${file_first}" != "${explicit_first}" || {
    echo "not ok" 1 - "dedicated background-policy did not select priority"
    exit 0
}

echo "ok" 1 - "background-policy overrides loader and environment defaults"
exit 0
