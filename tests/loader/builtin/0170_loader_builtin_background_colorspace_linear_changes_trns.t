#!/bin/sh
# Verify linear background interpretation changes tRNS composition in builtin path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
output_gamma="${ARTIFACT_LOCAL_DIR}/builtin_bgcs_trns_gamma.six"
output_linear="${ARTIFACT_LOCAL_DIR}/builtin_bgcs_trns_linear.six"

run_img2sixel --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Lbuiltin:cms_engine=none! \
              -B#808080 "${input_png}" >"${output_gamma}" || {
    echo "not ok 1 - builtin tRNS gamma background composition failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_BACKGROUND_COLORSPACE=linear \
              -Lbuiltin:cms_engine=none! \
              -B#808080 "${input_png}" >"${output_linear}" || {
    echo "not ok 1 - builtin tRNS linear background composition failed"
    exit 0
}

if cmp -s "${output_gamma}" "${output_linear}"; then
    echo "not ok 1 - builtin gamma and linear tRNS composition produced identical output"
else
    echo "ok 1 - builtin linear background interpretation changes tRNS composition"
fi

exit 0
