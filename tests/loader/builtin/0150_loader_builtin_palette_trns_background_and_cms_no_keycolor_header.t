#!/bin/sh
# Verify builtin palette+tRNS drops keycolor header under cms=1 background.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn3p08.png"
out="${ARTIFACT_LOCAL_DIR}/builtin-palette-trns-cms1-white-tbbn3p08.six"
keycolor_header="$(printf '\033P0;1q')"
out_payload=''
out_line=''
out_has_header=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! \
              -B#ffffff \
              -d fs -y raster \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - builtin cms=1 background render failed"
    exit 0
}

while IFS= read -r out_line || test -n "${out_line}"; do
    out_payload="${out_payload}${out_line}
"
done < "${out}"

case "${out_payload}" in
    *"${keycolor_header}"*)
        out_has_header=1
        ;;
esac

test "${out_has_header}" = 0 || {
    echo "not ok 1 - builtin palette+tRNS kept keycolor header under cms=1 background"
    exit 0
}

echo "ok 1 - builtin palette+tRNS drops keycolor header under cms=1 background"

exit 0
