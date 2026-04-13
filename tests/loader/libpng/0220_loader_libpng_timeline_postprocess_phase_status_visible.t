#!/bin/sh
# TAP test: libpng timeline reports post-process phases as finish/skip.

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

alpha_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-rgba16-alpha.png"
icc_png="${TOP_SRCDIR}/tests/data/colormgmt/input/png/rgb/img_rgb_icc1_srgb0_chrm0_gama0.png"
alpha_log="${ARTIFACT_LOCAL_DIR}/timeline-libpng-post-alpha.json"
icc_log="${ARTIFACT_LOCAL_DIR}/timeline-libpng-post-icc.json"

SIXEL_LOG_PATH="${alpha_log}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${alpha_png}" >/dev/null || {
    echo "not ok 1 - alpha conversion failed while capturing timeline"
    exit 0
}

SIXEL_LOG_PATH="${icc_log}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${icc_png}" >/dev/null || {
    echo "not ok 1 - ICC conversion failed while capturing timeline"
    exit 0
}

awk '
/"worker":"loader\/libpng"/ &&
/"role":"post\/background"/ &&
/"event":"(finish|skip)"/ { ok = 1 }
END { exit !ok }
' "${alpha_log}" || {
    echo "not ok 1 - post/background status was not visible"
    exit 0
}

awk '
/"worker":"loader\/libpng"/ &&
/"role":"post\/colorspace"/ &&
/"event":"(finish|skip)"/ { ok = 1 }
END { exit !ok }
' "${alpha_log}" || {
    echo "not ok 1 - post/colorspace status was not visible"
    exit 0
}

awk '
/"worker":"loader\/libpng"/ &&
/"role":"post\/icc"/ &&
/"event":"(finish|skip)"/ { ok = 1 }
END { exit !ok }
' "${icc_log}" || {
    echo "not ok 1 - post/icc status was not visible"
    exit 0
}

echo "ok 1 - libpng timeline reports post-process status"
exit 0
