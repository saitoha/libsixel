#!/bin/sh
# Emit TAP for palette component boundary checks.

set -eu

src_root=${1:-}

echo "1..5"

if test -z "$src_root"; then
    echo "not ok 1 - palette header is opaque and factory-only"
    echo "# src_root argument is required"
    echo "not ok 2 - retired public palette APIs are absent"
    echo "# src_root argument is required"
    echo "not ok 3 - palette fields are private to palette implementation family"
    echo "# src_root argument is required"
    echo "not ok 4 - palette storage rejects foreign responsibilities"
    echo "# src_root argument is required"
    echo "not ok 5 - quant/palette classid is registered"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-palette-boundary-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

failed=0
header=$src_root/src/palette.h
private_header=$src_root/src/palette-private.h
classid_gperf=$src_root/src/classid-factory.gperf
header_violations=$tmpdir/palette-header-violations.txt
retired_uses=$tmpdir/palette-retired-uses.txt
field_uses=$tmpdir/palette-field-uses.txt
storage_violations=$tmpdir/palette-storage-violations.txt

if test ! -f "$header"; then
    echo "not ok 1 - palette header is opaque and factory-only"
    echo "# missing header: $header"
    failed=1
else
    awk '
    /^struct sixel_palette[[:space:]]*\{/ ||
    /sixel_palette_(new|ref|unref|resize|set_entries|set_entries_float32|copy_entries_8bit|copy_entries_float32)[[:space:]]*\(/ ||
    /sixel_dither_get_quantized_palette[[:space:]]*\(/ {
        print FILENAME ":" FNR ":" $0
    }
    ' "$header" > "$header_violations"
    if test -s "$header_violations"; then
        echo "not ok 1 - palette header is opaque and factory-only"
        sed 's/^/# palette header leak: /' "$header_violations"
        failed=1
    else
        echo "ok 1 - palette header is opaque and factory-only"
    fi
fi

find "$src_root/include" "$src_root/src" "$src_root/tests" "$src_root/perl" \
    "$src_root/converters" "$src_root/assessment" "$src_root/fuzz" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.idl' \
        -o -name '*.pm' \) \
    ! -path "$src_root/src/palette.c" \
    ! -path "$src_root/src/palette-*.c" \
    ! -path "$src_root/src/palette-private.h" \
    ! -path "$src_root/tests/_static/sh/staticcheck-palette-component-boundary.sh" \
    -exec awk '
    /sixel_palette_(new|ref|unref|resize|set_entries|set_entries_float32|copy_entries_8bit|copy_entries_float32)[[:space:]]*\(/ ||
    /sixel_dither_get_quantized_palette[[:space:]]*\(/ {
        print FILENAME ":" FNR ":" $0
    }
    ' {} + > "$retired_uses"

if test -s "$retired_uses"; then
    echo "not ok 2 - retired public palette APIs are absent"
    sed 's/^/# retired palette API: /' "$retired_uses"
    failed=1
else
    echo "ok 2 - retired public palette APIs are absent"
fi

find "$src_root/src" "$src_root/include" "$src_root/tests" \
    "$src_root/converters" "$src_root/assessment" "$src_root/fuzz" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc.c' \) \
    ! -path "$src_root/src/palette.c" \
    ! -path "$src_root/src/palette-*.c" \
    ! -path "$src_root/src/palette-private.h" \
    -exec awk '
    /(^|[^A-Za-z0-9_])([A-Za-z0-9_]*palette|ppalette)->(entries|entries_float32|entries_size|entries_float32_size|entry_count|requested_colors|original_colors|depth|float_depth|allocator|lookup_policy|dither_policy|input_pixels|method_for_largest|method_for_rep|quality_mode|force_palette|use_reversible|quantize_model|final_merge|final_merge_mode|lut_policy)([^A-Za-z0-9_]|$)/ {
        print FILENAME ":" FNR ":" $0
    }
    ' {} + > "$field_uses"

if test -s "$field_uses"; then
    echo "not ok 3 - palette fields are private to palette implementation family"
    sed 's/^/# direct palette field: /' "$field_uses"
    failed=1
else
    echo "ok 3 - palette fields are private to palette implementation family"
fi

if test ! -f "$private_header"; then
    echo "not ok 4 - palette storage rejects foreign responsibilities"
    echo "# missing private header: $private_header"
    failed=1
else
    awk '
    /^typedef struct sixel_palette_storage[[:space:]]*\{/ {
        in_storage = 1
        next
    }
    in_storage != 0 && /^\}/ {
        in_storage = 0
        next
    }
    in_storage != 0 &&
    /(lookup_policy|dither_policy|input_pixels|method_for_largest|method_for_rep|quality_mode|force_palette|use_reversible|quantize_model|final_merge_mode|lut_policy)/ {
        print FILENAME ":" FNR ":" $0
    }
    ' "$private_header" > "$storage_violations"
    if test -s "$storage_violations"; then
        echo "not ok 4 - palette storage rejects foreign responsibilities"
        sed 's/^/# palette storage leak: /' "$storage_violations"
        failed=1
    else
        echo "ok 4 - palette storage rejects foreign responsibilities"
    fi
fi

if test ! -f "$classid_gperf"; then
    echo "not ok 5 - quant/palette classid is registered"
    echo "# missing registry: $classid_gperf"
    failed=1
elif awk -F '[,[:space:]]+' '
$2 == "define" && $4 == "sixel_palette_factory_new" {
    palette_macro = $3
}
$1 == "quant/palette" && $2 == palette_macro {
    found = 1
}
END {
    exit found ? 0 : 1
}
' "$classid_gperf"; then
    echo "ok 5 - quant/palette classid is registered"
else
    echo "not ok 5 - quant/palette classid is registered"
    echo "# quant/palette -> sixel_palette_factory_new is missing"
    failed=1
fi

exit "$failed"
