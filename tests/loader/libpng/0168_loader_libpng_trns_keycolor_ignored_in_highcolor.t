#!/bin/sh
# Verify high-color mode ignores tRNS keycolor opt-in and keeps opaque header.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..4"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

extract_header_params() {
    perl -0777 -ne '
        if (/^\eP([^q]*)q/s) {
            print $1;
        }
    '
}

extract_first_body_glyph() {
    perl -0777 -ne '
        $s = $_;
        $s =~ s/^.*?\eP[^q]*q//s;
        $s =~ s/^"[0-9;]*//s;
        $s =~ s/^(?:#\d+;2;\d+;\d+;\d+)+//;
        if ($s =~ /^(?:#\d+(?:!\d+)?)?(?:!\d+)?([?-~])/s) {
            print $1;
        }
    '
}

files="tbbn0g04.png tbrn2c08.png"
index=0

for file in ${files}; do
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"

    index=$((index + 1))
    out0="${ARTIFACT_LOCAL_DIR}/trns-keycolor-highcolor-off-${file}.six"
    run_img2sixel -I \
                  --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  -Llibpng:cms=0! \
                  "${input_png}" >"${out0}" || {
        echo "not ok" "${index}" - "highcolor opt-out render failed (${file})"
        continue
    }
    hdr0="$(extract_header_params <"${out0}")"
    glyph0="$(extract_first_body_glyph <"${out0}")"
    if [ -z "${hdr0}" ] && [ "${glyph0}" = "~" ]; then
        echo "ok" "${index}" - "highcolor opt-out stays opaque (${file})"
    else
        echo "not ok" "${index}" - "highcolor opt-out unexpected keycolor (${file}, hdr=${hdr0}, glyph=${glyph0})"
    fi

    index=$((index + 1))
    out1="${ARTIFACT_LOCAL_DIR}/trns-keycolor-highcolor-on-${file}.six"
    run_img2sixel -I \
                  --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms=0! \
                  "${input_png}" >"${out1}" || {
        echo "not ok" "${index}" - "highcolor opt-in render failed (${file})"
        continue
    }
    hdr1="$(extract_header_params <"${out1}")"
    glyph1="$(extract_first_body_glyph <"${out1}")"
    if [ -z "${hdr1}" ] && [ "${glyph1}" = "~" ]; then
        echo "ok" "${index}" - "highcolor opt-in still ignores keycolor (${file})"
    else
        echo "not ok" "${index}" - "highcolor opt-in unexpectedly used keycolor (${file}, hdr=${hdr1}, glyph=${glyph1})"
    fi
done

exit 0
