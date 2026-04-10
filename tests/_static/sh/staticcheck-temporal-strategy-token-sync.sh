#!/bin/sh
# Emit TAP for temporal strategy token sync across source and tests.

set -eu

echo "1..1"

src_root=$1
source_file_h=$src_root/src/dither-temporal-method.h
source_file_c=$src_root/src/dither-temporal-method.c
temporal_tests_dir=$src_root/tests/processing/dither/temporal

if test ! -d "$temporal_tests_dir"; then
    echo "ok 1 # SKIP missing temporal strategy source or test directory"
    exit 0
fi
if test ! -f "$source_file_h" && test ! -f "$source_file_c"; then
    echo "ok 1 # SKIP missing temporal strategy source or test directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-temporal-token-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected_tokens=$tmpdir/expected_tokens.txt
source_tokens=$tmpdir/source_tokens.txt
expected_float32_tests=$tmpdir/expected_float32_tests.txt
missing=$tmpdir/missing.txt
status=0

cat > "$expected_tokens" <<'EOF'
diffusion
stbn
stbn-hash
stbn-mask
EOF

cat > "$expected_float32_tests" <<'EOF'
0016_temporal_diffusion_float32_accepts_scan_option.t
0017_temporal_diffusion_float32_matches_fs_for_static_start_frame.t
0018_temporal_diffusion_float32_accepts_animated_input_builtin_apng.t
0019_temporal_diffusion_float32_mapfile_capture_stable_output_builtin_apng.t
0020_temporal_diffusion_float32_thread_count_stable_output_builtin_apng.t
0021_temporal_stbn_hash_changes_output_vs_diffusion_float32_animated_gif.t
0022_temporal_strategy_override_unknown_falls_back_to_diffusion_float32_builtin_apng.t
0023_temporal_stbn_mask_source_changes_output_vs_hash_float32_animated_gif.t
0024_temporal_stbn_hash_alias_matches_stbn_float32_builtin_apng.t
0025_temporal_stbn_hash_float32_mapfile_capture_stable_output_builtin_apng.t
0026_temporal_stbn_hash_float32_thread_count_stable_output_builtin_apng.t
0027_temporal_stbn_mask_float32_mapfile_capture_repeatable_output_builtin_apng.t
0028_temporal_stbn_mask_float32_thread_count_stable_output_builtin_apng.t
EOF

cat "$source_file_h" "$source_file_c" 2>/dev/null | awk '
/^sixel_temporal_strategy_token_from_string\(/ {
    in_func = 1
    next
}
in_func && /strcmp\(value, "/ {
    token = $0
    sub(/.*strcmp\(value, "/, "", token)
    sub(/".*/, "", token)
    if (token != "") {
        print token
    }
}
in_func && /^}/ {
    in_func = 0
}
' | LC_ALL=C sort -u > "$source_tokens"

while IFS= read -r token; do
    test -n "$token" || continue
    if ! grep -Fxq "$token" "$source_tokens"; then
        echo "# src/dither-temporal-method.[ch]: missing strategy token: $token" \
            >> "$missing"
        status=1
    fi
    if ! find "$temporal_tests_dir" -type f -name '*.t' -exec \
            grep -F "SIXEL_TEMPORAL_STRATEGY=$token" {} + >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: missing strategy token use: $token" \
            >> "$missing"
        status=1
    fi
done < "$expected_tokens"

while IFS= read -r token; do
    test -n "$token" || continue
    if ! grep -Fxq "$token" "$expected_tokens"; then
        echo "# src/dither-temporal-method.[ch]: unexpected strategy token: $token" \
            >> "$missing"
        status=1
    fi
done < "$source_tokens"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$temporal_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/temporal: missing float32 temporal test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: missing --precision=float32 in $test_name" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "-d temporal-diffusion" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: missing temporal-diffusion mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_float32_tests"

if test "$status" -eq 0; then
    echo "ok 1 - temporal strategy tokens stay synchronized"
    exit 0
fi

echo "not ok 1 - temporal strategy tokens stay synchronized"
cat "$missing"
exit 1
