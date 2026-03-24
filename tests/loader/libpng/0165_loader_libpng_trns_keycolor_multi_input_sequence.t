#!/bin/sh
# Verify tRNS keycolor behavior remains stable across multi-input sequences.

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

echo "1..12"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

extract_block_header_glyph_lines() {
    perl -0777 -ne '
        @blocks = ($_ =~ /(\eP.*?\e\\)/sg);
        for $block (@blocks) {
            $header = "";
            $glyph = "";
            if ($block =~ /^\eP([^q]*)q/s) {
                $header = $1;
            }
            $body = $block;
            $body =~ s/^.*?\eP[^q]*q//s;
            $body =~ s/^"[0-9;]*//s;
            $body =~ s/^(?:#\d+;2;\d+;\d+;\d+)+//;
            if ($body =~ /^(?:#\d+(?:!\d+)?)?(?:!\d+)?([?-~])/s) {
                $glyph = $1;
            }
            print $header . "\t" . $glyph . "\n";
        }
    '
}

input_dir="${TOP_SRCDIR}/images/pngsuite/transparency"
inputs="${input_dir}/tbbn0g04.png ${input_dir}/tbwn0g16.png ${input_dir}/tbrn2c08.png ${input_dir}/tbbn2c16.png ${input_dir}/tbgn2c16.png"

index=0

out_on="${ARTIFACT_LOCAL_DIR}/trns-keycolor-multi-on.six"
run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms=0! \
              -d fs -y raster \
              ${inputs} >"${out_on}" || {
    echo "not ok" 1 - "multi-input opt-in render failed"
    printf "not ok 2 - multi-input opt-out render skipped\n"
    exit 0
}

out_off="${ARTIFACT_LOCAL_DIR}/trns-keycolor-multi-off.six"
run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms=0! \
              -d fs -y raster \
              ${inputs} >"${out_off}" || {
    echo "not ok" 1 - "multi-input opt-in render passed"
    echo "not ok" 2 - "multi-input opt-out render failed"
    exit 0
}

on_lines="$(extract_block_header_glyph_lines <"${out_on}")"
off_lines="$(extract_block_header_glyph_lines <"${out_off}")"

on_count="$(printf '%s\n' "${on_lines}" | sed '/^$/d' | wc -l | tr -d '[:space:]')"
off_count="$(printf '%s\n' "${off_lines}" | sed '/^$/d' | wc -l | tr -d '[:space:]')"

index=$((index + 1))
if [ "${on_count}" = "5" ]; then
    echo "ok" "${index}" - "multi-input opt-in produced 5 sixel blocks"
else
    echo "not ok" "${index}" - "multi-input opt-in block count mismatch (got=${on_count})"
fi

index=$((index + 1))
if [ "${off_count}" = "5" ]; then
    echo "ok" "${index}" - "multi-input opt-out produced 5 sixel blocks"
else
    echo "not ok" "${index}" - "multi-input opt-out block count mismatch (got=${off_count})"
fi

for nth in 1 2 3 4 5; do
    index=$((index + 1))
    line="$(printf '%s\n' "${on_lines}" | sed -n "${nth}p")"
    hdr="$(printf '%s\n' "${line}" | awk -F '\t' '{print $1}')"
    glyph="$(printf '%s\n' "${line}" | awk -F '\t' '{print $2}')"
    if [ "${hdr}" = "0;1" ] && [ "${glyph}" = "?" ]; then
        echo "ok" "${index}" - "opt-in block ${nth} keeps keycolor"
    else
        echo "not ok" "${index}" - "opt-in block ${nth} mismatch (hdr=${hdr}, glyph=${glyph})"
    fi
done

for nth in 1 2 3 4 5; do
    index=$((index + 1))
    line="$(printf '%s\n' "${off_lines}" | sed -n "${nth}p")"
    hdr="$(printf '%s\n' "${line}" | awk -F '\t' '{print $1}')"
    glyph="$(printf '%s\n' "${line}" | awk -F '\t' '{print $2}')"
    if [ -z "${hdr}" ] && [ "${glyph}" = "~" ]; then
        echo "ok" "${index}" - "opt-out block ${nth} remains opaque"
    else
        echo "not ok" "${index}" - "opt-out block ${nth} mismatch (hdr=${hdr}, glyph=${glyph})"
    fi
done

exit 0
