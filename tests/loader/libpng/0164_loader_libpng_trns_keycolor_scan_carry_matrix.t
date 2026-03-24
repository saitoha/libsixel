#!/bin/sh
# Verify tRNS keycolor survives scan/carry combinations for fs/lso2.

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

echo "1..32"
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
    for precision in 8bit float32; do
        for diff in fs lso2; do
            for scan in raster serpentine; do
                for carry in direct carry; do
                    index=$((index + 1))
                    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-scan-carry-${precision}-${diff}-${scan}-${carry}-${file}.six"

                    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                                  --env SIXEL_THREADS=4 \
                                  -Llibpng:cms=0! \
                                  --precision="${precision}" \
                                  -d "${diff}" -y "${scan}" -Y "${carry}" \
                                  "${input_png}" >"${output_six}" || {
                        echo "not ok" "${index}" - "render failed (${precision}, ${diff}, ${scan}, ${carry}, ${file})"
                        continue
                    }

                    hdr="$(extract_header_params <"${output_six}")"
                    glyph="$(extract_first_body_glyph <"${output_six}")"
                    if [ "${hdr}" = "0;1" ] && [ "${glyph}" = "?" ]; then
                        echo "ok" "${index}" - "keycolor kept (${precision}, ${diff}, ${scan}, ${carry}, ${file})"
                    else
                        echo "not ok" "${index}" - "keycolor lost (${precision}, ${diff}, ${scan}, ${carry}, ${file}, hdr=${hdr}, glyph=${glyph})"
                    fi
                done
            done
        done
    done
done

exit 0
