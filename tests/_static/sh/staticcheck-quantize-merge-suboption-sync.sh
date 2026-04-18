#!/bin/sh
# Emit TAP for quantize suboption/env/help/schema consistency.

set -eu

echo "1..1"

src_root=$1
encoder_file=$src_root/src/encoder.c
help_file=$src_root/converters/img2sixel.c

if test ! -f "$encoder_file" || test ! -f "$help_file"; then
    echo "ok 1 # SKIP missing src/encoder.c or converters/img2sixel.c"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-merge-subopt-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

merge_only_keys=$tmpdir/merge_only_keys.txt
kmeans_keys=$tmpdir/kmeans_keys.txt
kmedoids_keys=$tmpdir/kmedoids_keys.txt
center_keys=$tmpdir/center_keys.txt
merge_only_pairs=$tmpdir/merge_only_pairs.tsv
kmeans_pairs=$tmpdir/kmeans_pairs.tsv
kmedoids_pairs=$tmpdir/kmedoids_pairs.tsv
center_pairs=$tmpdir/center_pairs.tsv
help_vars=$tmpdir/help_vars.txt
missing=$tmpdir/missing.txt

extract_keys() {
    block_name=$1
    awk -v block="$block_name" '
    $0 ~ ("static sixel_suboption_key_t const " block "\\[\\][[:space:]]*=[[:space:]]*\\{") {
        in_block = 1
        field_index = 0
        next
    }
    in_block && /^[[:space:]]*};/ {
        in_block = 0
        next
    }
    !in_block { next }
    {
        line = $0
        if (line ~ /^[[:space:]]*\{[[:space:]]*$/) {
            field_index = 0
            next
        }
        if (line ~ /^[[:space:]]*"[^"]+",[[:space:]]*$/) {
            token = line
            sub(/^[[:space:]]*"/, "", token)
            sub(/",.*/, "", token)
            field_index += 1
            if (field_index == 1) {
                print token
            }
            next
        }
        if (line ~ /^[[:space:]]*NULL,[[:space:]]*$/) {
            field_index += 1
        }
    }
    ' "$encoder_file" | LC_ALL=C sort -u
}

extract_merge_pairs() {
    block_name=$1
    awk -v block="$block_name" '
    $0 ~ ("static sixel_suboption_key_t const " block "\\[\\][[:space:]]*=[[:space:]]*\\{") {
        in_block = 1
        key = ""
        field_index = 0
        next
    }
    in_block && /^[[:space:]]*};/ {
        in_block = 0
        next
    }
    !in_block { next }
    {
        line = $0
        if (line ~ /^[[:space:]]*\{[[:space:]]*$/) {
            key = ""
            field_index = 0
            next
        }
        if (line ~ /^[[:space:]]*"[^"]+",[[:space:]]*$/) {
            token = line
            sub(/^[[:space:]]*"/, "", token)
            sub(/",.*/, "", token)
            field_index += 1
            if (field_index == 1) {
                key = token
            } else if (field_index == 3) {
                if (key != "") {
                    if (token ~ /^SIXEL_PALETTE_(ANIMATION_MODE|SCENE_CUT_THRESHOLD|OVERSPLIT_FACTOR|FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT)$/) {
                        printf "%s\t%s\n", key, token
                        key = ""
                    }
                }
            }
            next
        }
        if (line ~ /^[[:space:]]*SIXEL_PALETTE_(ANIMATION_MODE_ENVVAR|SCENE_CUT_THRESHOLD_ENVVAR),[[:space:]]*$/) {
            token = line
            sub(/^[[:space:]]*/, "", token)
            sub(/,[[:space:]]*$/, "", token)
            field_index += 1
            if (field_index == 3 && key != "") {
                if (token == "SIXEL_PALETTE_ANIMATION_MODE_ENVVAR") {
                    printf "%s\tSIXEL_PALETTE_ANIMATION_MODE\n", key
                    key = ""
                } else if (token == "SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR") {
                    printf "%s\tSIXEL_PALETTE_SCENE_CUT_THRESHOLD\n", key
                    key = ""
                }
            }
            next
        }
        if (line ~ /^[[:space:]]*NULL,[[:space:]]*$/) {
            field_index += 1
            if (field_index == 3) {
                key = ""
            }
        }
    }
    ' "$encoder_file" | LC_ALL=C sort -u
}

extract_keys g_subkeys_quantize_model_merge_only > "$merge_only_keys"
extract_keys g_subkeys_quantize_model_kmeans > "$kmeans_keys"
extract_keys g_subkeys_quantize_model_kmedoids > "$kmedoids_keys"
extract_keys g_subkeys_quantize_model_center > "$center_keys"

extract_merge_pairs g_subkeys_quantize_model_merge_only > "$merge_only_pairs"
extract_merge_pairs g_subkeys_quantize_model_kmeans > "$kmeans_pairs"
extract_merge_pairs g_subkeys_quantize_model_kmedoids > "$kmedoids_pairs"
extract_merge_pairs g_subkeys_quantize_model_center > "$center_pairs"

awk '
/^[[:space:]]*"SIXEL_PALETTE_(ANIMATION_MODE|SCENE_CUT_THRESHOLD|OVERSPLIT_FACTOR|FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT)"/ {
    line = $0
    sub(/^[[:space:]]*"/, "", line)
    sub(/".*$/, "", line)
    print line
}
' "$help_file" | LC_ALL=C sort -u > "$help_vars"

status=0

for key in animation_mode scene_cut_threshold merge merge_oversplit merge_lloyd; do
    grep -Fxq "$key" "$merge_only_keys" || {
        echo "# merge-only block missing key: $key" >> "$missing"
        status=1
    }
    grep -Fxq "$key" "$kmeans_keys" || {
        echo "# kmeans block missing merge key: $key" >> "$missing"
        status=1
    }
    grep -Fxq "$key" "$kmedoids_keys" || {
        echo "# kmedoids block missing merge key: $key" >> "$missing"
        status=1
    }
    grep -Fxq "$key" "$center_keys" || {
        echo "# center block missing merge key: $key" >> "$missing"
        status=1
    }
done

for pair in \
    "animation_mode	SIXEL_PALETTE_ANIMATION_MODE" \
    "scene_cut_threshold	SIXEL_PALETTE_SCENE_CUT_THRESHOLD" \
    "merge_oversplit	SIXEL_PALETTE_OVERSPLIT_FACTOR" \
    "merge_lloyd	SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT"; do
    grep -Fxq "$pair" "$merge_only_pairs" || {
        echo "# merge-only block missing pair: $pair" >> "$missing"
        status=1
    }
    grep -Fxq "$pair" "$kmeans_pairs" || {
        echo "# kmeans block missing merge pair: $pair" >> "$missing"
        status=1
    }
    grep -Fxq "$pair" "$kmedoids_pairs" || {
        echo "# kmedoids block missing merge pair: $pair" >> "$missing"
        status=1
    }
    grep -Fxq "$pair" "$center_pairs" || {
        echo "# center block missing merge pair: $pair" >> "$missing"
        status=1
    }
done

for env_name in \
    SIXEL_PALETTE_ANIMATION_MODE \
    SIXEL_PALETTE_SCENE_CUT_THRESHOLD \
    SIXEL_PALETTE_OVERSPLIT_FACTOR \
    SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT; do
    grep -Fxq "$env_name" "$help_vars" || {
        echo "# converters/img2sixel.c: missing env help entry: $env_name" \
            >> "$missing"
        status=1
    }
done

awk '
/g_schema_quantize_model_values\[\][[:space:]]*=[[:space:]]*\{/ {
    in_block = 1
    next
}
in_block && /^[[:space:]]*};/ {
    in_block = 0
    next
}
!in_block { next }
/^[[:space:]]*"auto",[[:space:]]*$/ {
    want_auto = 1
    next
}
/^[[:space:]]*"heckbert",[[:space:]]*$/ {
    want_heckbert = 1
    next
}
want_auto && /g_subkeys_quantize_model_merge_only/ {
    auto_ok = 1
    want_auto = 0
}
want_heckbert && /g_subkeys_quantize_model_merge_only/ {
    heckbert_ok = 1
    want_heckbert = 0
}
END {
    if (auto_ok && heckbert_ok) {
        exit 0
    }
    exit 1
}
' "$encoder_file" || {
    echo "# schema mismatch: auto/heckbert must reference merge-only subkeys" \
        >> "$missing"
    status=1
}

if test "$status" -eq 0; then
    echo "ok 1 - quantize suboptions stay in sync"
    exit 0
fi

echo "not ok 1 - quantize suboptions stay in sync"
cat "$missing"
exit 1
