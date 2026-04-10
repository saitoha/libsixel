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
expected_8bit_mask_tests=$tmpdir/expected_8bit_mask_tests.txt
expected_8bit_pmj_tests=$tmpdir/expected_8bit_pmj_tests.txt
missing=$tmpdir/missing.txt
status=0

cat > "$expected_tokens" <<'EOF'
diffusion
stbn
stbn-hash
stbn-mask
pmj
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
0031_temporal_strategy_cli_overrides_env_float32_animated_gif.t
0032_temporal_pmj_source_changes_output_vs_stbn_hash_float32_animated_gif.t
0033_temporal_pmj_float32_thread_count_stable_output_builtin_apng.t
0035_temporal_pmj_float32_mapfile_capture_repeatable_output_builtin_apng.t
EOF

cat > "$expected_8bit_mask_tests" <<'EOF'
0029_temporal_stbn_mask_mapfile_capture_repeatable_output_builtin_apng.t
0030_temporal_stbn_mask_thread_count_stable_output_builtin_apng.t
EOF

cat > "$expected_8bit_pmj_tests" <<'EOF'
0036_temporal_pmj_mapfile_capture_repeatable_output_builtin_apng.t
0037_temporal_pmj_thread_count_stable_output_builtin_apng.t
EOF

if grep -F -- "\"SIXEL_TEMPORAL_STRATEGY\"" "$source_file_h" "$source_file_c" \
        >/dev/null 2>&1; then
    echo "# src/dither-temporal-method.[ch]: legacy env name must not remain" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY" "$source_file_h" \
        >/dev/null 2>&1; then
    echo "# src/dither-temporal-method.h: missing env var macro name" \
        >> "$missing"
    status=1
fi

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
            grep -F "SIXEL_DITHER_TEMPORAL_STRATEGY=$token" {} + >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: missing strategy token use: $token" \
            >> "$missing"
        status=1
    fi
done < "$expected_tokens"

if find "$temporal_tests_dir" -type f -name '*.t' -exec \
        grep -F "SIXEL_TEMPORAL_STRATEGY=" {} + >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: legacy env name must not remain" \
        >> "$missing"
    status=1
fi

if ! find "$temporal_tests_dir" -type f -name '*.t' -exec \
        grep -F -- "-d temporal-diffusion:strategy=" {} + \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: missing CLI strategy suboption coverage" \
        >> "$missing"
    status=1
fi

test_path="$src_root/tests/cli/options/matching/0130_option_matching_diffusion_temporal_strategy_unknown_key.t"
if test ! -f "$test_path"; then
    echo "# tests/cli/options/matching: missing unknown strategy key coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "temporal-diffusion:mode=" "$test_path" >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0130 must exercise unknown key path" \
            >> "$missing"
        status=1
    fi
fi

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

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$temporal_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/temporal: missing 8bit stbn-mask test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-mask" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: missing stbn-mask strategy in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: unexpected float32 mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_8bit_mask_tests"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$temporal_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/temporal: missing 8bit pmj test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: missing pmj strategy in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: unexpected float32 mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_8bit_pmj_tests"

if ! grep -F -- "-M " \
        "$temporal_tests_dir/0029_temporal_stbn_mask_mapfile_capture_repeatable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0029 must exercise mapfile capture (-M)" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "--threads=2" \
        "$temporal_tests_dir/0030_temporal_stbn_mask_thread_count_stable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0030 must compare --threads=1 and --threads=2" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "-M " \
        "$temporal_tests_dir/0036_temporal_pmj_mapfile_capture_repeatable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0036 must exercise mapfile capture (-M)" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "--threads=2" \
        "$temporal_tests_dir/0037_temporal_pmj_thread_count_stable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0037 must compare --threads=1 and --threads=2" \
        >> "$missing"
    status=1
fi

test_path="$temporal_tests_dir/0034_temporal_pmj_source_changes_output_vs_stbn_hash_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/temporal: missing 8bit pmj coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=pmj" "$test_path" \
            >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: 0034 must exercise pmj strategy" \
            >> "$missing"
        status=1
    fi
fi

test_path="$temporal_tests_dir/0012_temporal_stbn_placeholder_matches_diffusion_builtin_apng.t"
if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=diffusion" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=stbn" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0012 must keep 8bit stbn=diffusion coverage" \
        >> "$missing"
    status=1
fi
if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0012 must remain 8bit coverage" \
        >> "$missing"
    status=1
fi

test_path="$temporal_tests_dir/0014_temporal_stbn_mask_source_changes_output_vs_hash_animated_gif.t"
if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-mask" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-hash" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0014 must keep 8bit stbn-mask!=stbn-hash coverage" \
        >> "$missing"
    status=1
fi
if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0014 must remain 8bit coverage" \
        >> "$missing"
    status=1
fi

test_path="$temporal_tests_dir/0021_temporal_stbn_hash_changes_output_vs_diffusion_float32_animated_gif.t"
if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-hash" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=diffusion" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0021 must keep float32 stbn-hash!=diffusion coverage" \
        >> "$missing"
    status=1
fi
if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0021 must remain float32 coverage" \
        >> "$missing"
    status=1
fi

test_path="$temporal_tests_dir/0023_temporal_stbn_mask_source_changes_output_vs_hash_float32_animated_gif.t"
if ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-mask" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-hash" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0023 must keep float32 stbn-mask!=stbn-hash coverage" \
        >> "$missing"
    status=1
fi
if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/temporal: 0023 must remain float32 coverage" \
        >> "$missing"
    status=1
fi

if test "$status" -eq 0; then
    echo "ok 1 - temporal strategy tokens stay synchronized"
    exit 0
fi

echo "not ok 1 - temporal strategy tokens stay synchronized"
cat "$missing"
exit 1
