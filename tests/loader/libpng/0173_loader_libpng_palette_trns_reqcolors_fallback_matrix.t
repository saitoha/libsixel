#!/bin/sh
# Verify indexed+tRNS keeps PAL8 at high reqcolors and falls back at low reqcolors.

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

echo "1..10"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

extract_leading_glyph() {
    perl -0777 -ne '
        $s = $_;
        $s =~ s/^.*?(?=#\d+;2;)//s;
        $s =~ s/^(?:#\d+;2;\d+;\d+;\d+)+//;
        if ($s =~ /^#\d+(?:!\d+)?(.)/s) {
            print $1;
        }
    '
}

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
index=0

for cms in 0 1; do
    out_hi="${ARTIFACT_LOCAL_DIR}/req-hi-cms${cms}.six"
    log_hi="${ARTIFACT_LOCAL_DIR}/req-hi-cms${cms}.log"
    run_img2sixel -v -Llibpng:cms=${cms}! \
                  -B#ffffff -d none -p256 \
                  "${input_png}" >"${out_hi}" 2>"${log_hi}" || {
        index=$((index + 1))
        echo "not ok" "${index}" - "high-reqcolors render failed (cms=${cms})"
        index=$((index + 1))
        echo "not ok" "${index}" - "high-reqcolors transparency check skipped (cms=${cms})"
        index=$((index + 1))
        echo "not ok" "${index}" - "low-reqcolors format check skipped (cms=${cms})"
        index=$((index + 1))
        echo "not ok" "${index}" - "low-reqcolors transparency check skipped (cms=${cms})"
        index=$((index + 1))
        echo "not ok" "${index}" - "reqcolors diff check skipped (cms=${cms})"
        continue
    }

    index=$((index + 1))
    if grep -q 'formats: source=pal8 work=pal8' "${log_hi}"; then
        echo "ok" "${index}" - "high reqcolors keeps PAL8 path (cms=${cms})"
    else
        echo "not ok" "${index}" - "high reqcolors did not keep PAL8 path (cms=${cms})"
    fi

    index=$((index + 1))
    glyph_hi="$(extract_leading_glyph <"${out_hi}")"
    if [ "${glyph_hi}" = "?" ]; then
        echo "ok" "${index}" - "high reqcolors keeps transparent keycolor (cms=${cms})"
    else
        echo "not ok" "${index}" - "high reqcolors lost transparent keycolor (cms=${cms}, got=${glyph_hi})"
    fi

    out_lo="${ARTIFACT_LOCAL_DIR}/req-lo-cms${cms}.six"
    log_lo="${ARTIFACT_LOCAL_DIR}/req-lo-cms${cms}.log"
    run_img2sixel -v -Llibpng:cms=${cms}! \
                  -B#ffffff -d none -p16 \
                  "${input_png}" >"${out_lo}" 2>"${log_lo}" || {
        index=$((index + 1))
        echo "not ok" "${index}" - "low-reqcolors render failed (cms=${cms})"
        index=$((index + 1))
        echo "not ok" "${index}" - "low-reqcolors transparency check skipped (cms=${cms})"
        index=$((index + 1))
        echo "not ok" "${index}" - "reqcolors diff check skipped (cms=${cms})"
        continue
    }

    index=$((index + 1))
    if grep -q 'formats: source=pal8 work=pal8' "${log_lo}"; then
        echo "not ok" "${index}" - "low reqcolors unexpectedly stayed on PAL8 path (cms=${cms})"
    else
        echo "ok" "${index}" - "low reqcolors moved off PAL8 path (cms=${cms})"
    fi

    index=$((index + 1))
    glyph_lo="$(extract_leading_glyph <"${out_lo}")"
    if [ "${glyph_lo}" = "?" ]; then
        echo "not ok" "${index}" - "low reqcolors unexpectedly kept transparent keycolor (cms=${cms})"
    else
        echo "ok" "${index}" - "low reqcolors disables transparent keycolor path (cms=${cms})"
    fi

    index=$((index + 1))
    if cmp -s "${out_hi}" "${out_lo}"; then
        echo "not ok" "${index}" - "high/low reqcolors outputs unexpectedly identical (cms=${cms})"
    else
        echo "ok" "${index}" - "high/low reqcolors outputs differ as expected (cms=${cms})"
    fi
done

exit 0
