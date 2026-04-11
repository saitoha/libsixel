#!/bin/sh
# Emit TAP for interframe strategy token sync across source and tests.

set -eu

echo "1..1"

src_root=$1
source_file_h=$src_root/src/dither-interframe-method.h
source_file_c=$src_root/src/dither-interframe-method.c
interframe_tests_dir=$src_root/tests/processing/dither/interframe

if test ! -d "$interframe_tests_dir"; then
    echo "ok 1 # SKIP missing interframe strategy source or test directory"
    exit 0
fi
if test ! -f "$source_file_h" && test ! -f "$source_file_c"; then
    echo "ok 1 # SKIP missing interframe strategy source or test directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-interframe-token-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected_tokens=$tmpdir/expected_tokens.txt
source_tokens=$tmpdir/source_tokens.txt
expected_float32_tests=$tmpdir/expected_float32_tests.txt
expected_8bit_mask_tests=$tmpdir/expected_8bit_mask_tests.txt
expected_8bit_pmj_tests=$tmpdir/expected_8bit_pmj_tests.txt
expected_8bit_pmj_cli_tests=$tmpdir/expected_8bit_pmj_cli_tests.txt
expected_float32_pmj_cli_tests=$tmpdir/expected_float32_pmj_cli_tests.txt
expected_8bit_mask_cli_tests=$tmpdir/expected_8bit_mask_cli_tests.txt
legacy_alias_prefix=interframe
legacy_alias_suffix=diffusion
legacy_alias_token=
missing=$tmpdir/missing.txt
status=0

legacy_alias_token="${legacy_alias_prefix}-${legacy_alias_suffix}"

cat > "$expected_tokens" <<'EOF'
diffusion
stbn
stbn-hash
stbn-mask
pmj
EOF

cat > "$expected_float32_tests" <<'EOF'
0016_interframe_diffusion_float32_accepts_scan_option.t
0017_interframe_diffusion_float32_matches_fs_for_static_start_frame.t
0018_interframe_diffusion_float32_accepts_animated_input_builtin_apng.t
0019_interframe_diffusion_float32_mapfile_capture_stable_output_builtin_apng.t
0020_interframe_diffusion_float32_thread_count_stable_output_builtin_apng.t
0021_interframe_stbn_hash_changes_output_vs_diffusion_float32_animated_gif.t
0022_interframe_strategy_override_unknown_falls_back_to_diffusion_float32_builtin_apng.t
0023_interframe_stbn_mask_source_changes_output_vs_hash_float32_animated_gif.t
0024_interframe_stbn_hash_alias_matches_stbn_float32_builtin_apng.t
0025_interframe_stbn_hash_float32_mapfile_capture_stable_output_builtin_apng.t
0026_interframe_stbn_hash_float32_thread_count_stable_output_builtin_apng.t
0027_interframe_stbn_mask_float32_mapfile_capture_repeatable_output_builtin_apng.t
0028_interframe_stbn_mask_float32_thread_count_stable_output_builtin_apng.t
0031_interframe_strategy_cli_overrides_env_float32_animated_gif.t
0032_interframe_pmj_source_changes_output_vs_stbn_hash_float32_animated_gif.t
0033_interframe_pmj_float32_thread_count_stable_output_builtin_apng.t
0035_interframe_pmj_float32_mapfile_capture_repeatable_output_builtin_apng.t
0041_interframe_strategy_cli_overrides_env_float32_pmj_animated_gif.t
0042_interframe_pmj_cli_float32_mapfile_capture_repeatable_output_builtin_apng.t
0043_interframe_pmj_cli_float32_thread_count_stable_output_builtin_apng.t
0045_interframe_strategy_cli_diffusion_matches_default_float32_animated_gif.t
0047_interframe_strategy_cli_stbn_alias_matches_hash_float32_animated_gif.t
0051_interframe_pmj_source_changes_output_vs_stbn_hash_float32_builtin_apng.t
0053_interframe_strategy_cli_pmj_resets_across_size_change_float32_builtin_mixed.t
0055_interframe_pmj_strength_zero_matches_diffusion_float32_builtin_apng.t
EOF

cat > "$expected_8bit_mask_tests" <<'EOF'
0029_interframe_stbn_mask_mapfile_capture_repeatable_output_builtin_apng.t
0030_interframe_stbn_mask_thread_count_stable_output_builtin_apng.t
0054_interframe_stbn_mask_strength_zero_matches_diffusion_builtin_apng.t
EOF

cat > "$expected_8bit_pmj_tests" <<'EOF'
0036_interframe_pmj_mapfile_capture_repeatable_output_builtin_apng.t
0037_interframe_pmj_thread_count_stable_output_builtin_apng.t
0050_interframe_pmj_source_changes_output_vs_stbn_hash_builtin_apng.t
EOF

cat > "$expected_8bit_pmj_cli_tests" <<'EOF'
0039_interframe_pmj_cli_mapfile_capture_repeatable_output_builtin_apng.t
0040_interframe_pmj_cli_thread_count_stable_output_builtin_apng.t
0048_interframe_strategy_cli_pmj_resets_between_inputs_builtin_apng.t
0052_interframe_strategy_cli_pmj_resets_across_size_change_builtin_mixed.t
EOF

cat > "$expected_float32_pmj_cli_tests" <<'EOF'
0042_interframe_pmj_cli_float32_mapfile_capture_repeatable_output_builtin_apng.t
0043_interframe_pmj_cli_float32_thread_count_stable_output_builtin_apng.t
0053_interframe_strategy_cli_pmj_resets_across_size_change_float32_builtin_mixed.t
0055_interframe_pmj_strength_zero_matches_diffusion_float32_builtin_apng.t
EOF

cat > "$expected_8bit_mask_cli_tests" <<'EOF'
0049_interframe_strategy_cli_stbn_mask_resets_across_size_change_builtin_mixed.t
EOF

if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY" "$source_file_h" \
        >/dev/null 2>&1; then
    echo "# src/dither-interframe-method.h: missing env var macro name" \
        >> "$missing"
    status=1
fi
if ! grep -F -- "SIXEL_DITHER_INTERFRAME_NOISE_STRENGTH" "$source_file_h" \
        >/dev/null 2>&1; then
    echo "# src/dither-interframe-method.h: missing noise strength env macro" \
        >> "$missing"
    status=1
fi

cat "$source_file_h" "$source_file_c" 2>/dev/null | awk '
/^sixel_interframe_strategy_token_from_string\(/ {
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
        echo "# src/dither-interframe-method.[ch]: missing strategy token: $token" \
            >> "$missing"
        status=1
    fi
    if ! find "$interframe_tests_dir" -type f -name '*.t' -exec \
            grep -F "SIXEL_DITHER_INTERFRAME_STRATEGY=$token" {} + >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing strategy token use: $token" \
            >> "$missing"
        status=1
    fi
done < "$expected_tokens"

if ! find "$interframe_tests_dir" -type f -name '*.t' -exec \
        grep -F -- "-d interframe:strategy=" {} + \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: missing CLI strategy suboption coverage" \
        >> "$missing"
    status=1
fi

test_path="$src_root/tests/cli/options/matching/0130_option_matching_diffusion_interframe_strategy_unknown_key.t"
if test ! -f "$test_path"; then
    echo "# tests/cli/options/matching: missing unknown strategy key coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "interframe:mode=" "$test_path" >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0130 must exercise unknown key path" \
            >> "$missing"
        status=1
    fi
fi

test_path="$src_root/tests/cli/options/matching/0131_option_matching_diffusion_interframe_strategy_pmj_success.t"
if test ! -f "$test_path"; then
    echo "# tests/cli/options/matching: missing strategy=pmj success coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "interframe:strategy=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0131 must exercise strategy=pmj" \
            >> "$missing"
        status=1
    fi
fi

test_path="$src_root/tests/cli/options/matching/0133_option_matching_diffusion_interframe_noise_strength_suboption_success.t"
if test ! -f "$test_path"; then
    echo "# tests/cli/options/matching: missing noise_strength success coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "interframe:strategy=stbn-mask:noise_strength=" \
            "$test_path" >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0133 must exercise noise_strength suboption" \
            >> "$missing"
        status=1
    fi
fi

test_path="$src_root/tests/cli/options/matching/0134_option_matching_diffusion_non_interframe_rejects_noise_strength_suboption.t"
if test ! -f "$test_path"; then
    echo "# tests/cli/options/matching: missing non-interframe noise_strength rejection test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "fs:noise_strength=" "$test_path" >/dev/null 2>&1 \
            || ! grep -F -- "supported only for interframe" "$test_path" \
            >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0134 must reject noise_strength outside interframe" \
            >> "$missing"
        status=1
    fi
fi

test_path="$src_root/tests/cli/options/matching/0135_option_matching_diffusion_interframe_noise_strength_invalid_value.t"
if test ! -f "$test_path"; then
    echo "# tests/cli/options/matching: missing invalid noise_strength value test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "interframe:noise_strength=invalid" "$test_path" \
            >/dev/null 2>&1 \
            || ! grep -F -- "0.0-2.0" "$test_path" >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0135 must validate noise_strength range diagnostics" \
            >> "$missing"
        status=1
    fi
fi

test_path="$src_root/tests/cli/options/matching/0132_option_matching_diffusion_unknown_base_value_rejected.t"
if test ! -f "$test_path"; then
    echo "# tests/cli/options/matching: missing unknown diffusion base rejection test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "definitely-unknown-diffusion" "$test_path" \
            >/dev/null 2>&1 \
            || ! grep -F -- "unknown" "$test_path" >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0132 must reject unknown diffusion values" \
            >> "$missing"
        status=1
    fi
fi

if find "$interframe_tests_dir" -type f -name '*.t' -exec \
        grep -F -- "$legacy_alias_token" {} + >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: legacy diffusion alias must not remain" \
        >> "$missing"
    status=1
fi

if find "$src_root/tests/cli/options/matching" -type f -name '*.t' -exec \
        grep -F -- "$legacy_alias_token" {} + >/dev/null 2>&1; then
    echo "# tests/cli/options/matching: legacy diffusion alias must not remain" \
        >> "$missing"
    status=1
fi

while IFS= read -r token; do
    test -n "$token" || continue
    if ! grep -Fxq "$token" "$expected_tokens"; then
        echo "# src/dither-interframe-method.[ch]: unexpected strategy token: $token" \
            >> "$missing"
        status=1
    fi
done < "$source_tokens"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$interframe_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/interframe: missing float32 interframe test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing --precision=float32 in $test_name" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "-d interframe" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing interframe mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_float32_tests"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$interframe_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/interframe: missing 8bit stbn-mask test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-mask" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing stbn-mask strategy in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: unexpected float32 mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_8bit_mask_tests"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$interframe_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/interframe: missing 8bit pmj test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing pmj strategy in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: unexpected float32 mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_8bit_pmj_tests"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$interframe_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/interframe: missing 8bit pmj cli test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "interframe:strategy=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing cli strategy=pmj in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: unexpected env-based pmj in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: unexpected float32 mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_8bit_pmj_cli_tests"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$interframe_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/interframe: missing float32 pmj cli test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "interframe:strategy=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing cli strategy=pmj in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: unexpected env-based pmj in $test_name" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing --precision=float32 in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_float32_pmj_cli_tests"

while IFS= read -r test_name; do
    test -n "$test_name" || continue
    test_path="$interframe_tests_dir/$test_name"
    if test ! -f "$test_path"; then
        echo "# tests/processing/dither/interframe: missing 8bit stbn-mask cli test: $test_name" \
            >> "$missing"
        status=1
        continue
    fi
    if ! grep -F -- "interframe:strategy=stbn-mask" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: missing cli strategy=stbn-mask in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-mask" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: unexpected env-based stbn-mask in $test_name" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: unexpected float32 mode in $test_name" \
            >> "$missing"
        status=1
    fi
done < "$expected_8bit_mask_cli_tests"

if ! grep -F -- "-M " \
        "$interframe_tests_dir/0029_interframe_stbn_mask_mapfile_capture_repeatable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0029 must exercise mapfile capture (-M)" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "--threads=2" \
        "$interframe_tests_dir/0030_interframe_stbn_mask_thread_count_stable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0030 must compare --threads=1 and --threads=2" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "-M " \
        "$interframe_tests_dir/0036_interframe_pmj_mapfile_capture_repeatable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0036 must exercise mapfile capture (-M)" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "--threads=2" \
        "$interframe_tests_dir/0037_interframe_pmj_thread_count_stable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0037 must compare --threads=1 and --threads=2" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "-M " \
        "$interframe_tests_dir/0039_interframe_pmj_cli_mapfile_capture_repeatable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0039 must exercise mapfile capture (-M)" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "--threads=2" \
        "$interframe_tests_dir/0040_interframe_pmj_cli_thread_count_stable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0040 must compare --threads=1 and --threads=2" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "-M " \
        "$interframe_tests_dir/0042_interframe_pmj_cli_float32_mapfile_capture_repeatable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0042 must exercise mapfile capture (-M)" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "--threads=2" \
        "$interframe_tests_dir/0043_interframe_pmj_cli_float32_thread_count_stable_output_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0043 must compare --threads=1 and --threads=2" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "expected_output=\"\${single_output}\${single_output}\"" \
        "$interframe_tests_dir/0048_interframe_strategy_cli_pmj_resets_between_inputs_builtin_apng.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0048 must verify reset by concatenating single outputs" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "expected_output=\"\${animated_output}\${single_output}\"" \
        "$interframe_tests_dir/0049_interframe_strategy_cli_stbn_mask_resets_across_size_change_builtin_mixed.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0049 must verify reset by mixed-size concatenation" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "expected_output=\"\${animated_output}\${single_output}\"" \
        "$interframe_tests_dir/0052_interframe_strategy_cli_pmj_resets_across_size_change_builtin_mixed.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0052 must verify reset by mixed-size concatenation" \
        >> "$missing"
    status=1
fi

if ! grep -F -- "expected_output=\"\${animated_output}\${single_output}\"" \
        "$interframe_tests_dir/0053_interframe_strategy_cli_pmj_resets_across_size_change_float32_builtin_mixed.t" \
        >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0053 must verify reset by mixed-size concatenation" \
        >> "$missing"
    status=1
fi

test_path="$interframe_tests_dir/0034_interframe_pmj_source_changes_output_vs_stbn_hash_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing 8bit pmj coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=pmj" "$test_path" \
            >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0034 must exercise pmj strategy" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0050_interframe_pmj_source_changes_output_vs_stbn_hash_builtin_apng.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing 8bit small-frame pmj coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=pmj" "$test_path" \
            >/dev/null 2>&1 \
            || ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-hash" "$test_path" \
            >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0050 must compare pmj and stbn-hash" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "test \"\${pmj_output}\" != \"\${hash_output}\"" "$test_path" \
            >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0050 must enforce pmj!=stbn-hash" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0051_interframe_pmj_source_changes_output_vs_stbn_hash_float32_builtin_apng.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing float32 small-frame pmj coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=pmj" "$test_path" \
            >/dev/null 2>&1 \
            || ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-hash" "$test_path" \
            >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0051 must compare pmj and stbn-hash" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0051 must run in float32 mode" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "test \"\${pmj_output}\" != \"\${hash_output}\"" "$test_path" \
            >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0051 must enforce pmj!=stbn-hash" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0012_interframe_stbn_placeholder_matches_diffusion_builtin_apng.t"
if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=diffusion" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0012 must keep 8bit stbn=diffusion coverage" \
        >> "$missing"
    status=1
fi
if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0012 must remain 8bit coverage" \
        >> "$missing"
    status=1
fi

test_path="$interframe_tests_dir/0014_interframe_stbn_mask_source_changes_output_vs_hash_animated_gif.t"
if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-mask" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-hash" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0014 must keep 8bit stbn-mask!=stbn-hash coverage" \
        >> "$missing"
    status=1
fi
if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0014 must remain 8bit coverage" \
        >> "$missing"
    status=1
fi

test_path="$interframe_tests_dir/0021_interframe_stbn_hash_changes_output_vs_diffusion_float32_animated_gif.t"
if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-hash" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=diffusion" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0021 must keep float32 stbn-hash!=diffusion coverage" \
        >> "$missing"
    status=1
fi
if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0021 must remain float32 coverage" \
        >> "$missing"
    status=1
fi

test_path="$interframe_tests_dir/0023_interframe_stbn_mask_source_changes_output_vs_hash_float32_animated_gif.t"
if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-mask" "$test_path" >/dev/null 2>&1 \
        || ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-hash" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0023 must keep float32 stbn-mask!=stbn-hash coverage" \
        >> "$missing"
    status=1
fi
if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
    echo "# tests/processing/dither/interframe: 0023 must remain float32 coverage" \
        >> "$missing"
    status=1
fi

test_path="$interframe_tests_dir/0038_interframe_strategy_cli_overrides_env_8bit_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing 8bit cli override coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=diffusion" "$test_path" >/dev/null 2>&1 \
            || ! grep -F -- "interframe:strategy=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0038 must cover env/cli precedence for pmj" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0038 must remain 8bit coverage" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0041_interframe_strategy_cli_overrides_env_float32_pmj_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing float32 pmj cli override coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0041 must run in float32 mode" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=diffusion" "$test_path" >/dev/null 2>&1 \
            || ! grep -F -- "interframe:strategy=pmj" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0041 must cover float32 env/cli precedence for pmj" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0056_interframe_noise_strength_cli_overrides_env_8bit_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing cli noise_strength override coverage test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "SIXEL_DITHER_INTERFRAME_NOISE_STRENGTH=0" "$test_path" \
            >/dev/null 2>&1 \
            || ! grep -F -- "interframe:strategy=stbn-mask:noise_strength=" \
            "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0056 must cover cli/env noise_strength precedence" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0044_interframe_strategy_cli_diffusion_matches_default_8bit_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing 8bit strategy=diffusion cli/default equivalence test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "-d interframe -p 16" "$test_path" >/dev/null 2>&1 \
            || ! grep -F -- "interframe:strategy=diffusion" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0044 must compare default interframe vs strategy=diffusion" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0044 must remain 8bit coverage" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0045_interframe_strategy_cli_diffusion_matches_default_float32_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing float32 strategy=diffusion cli/default equivalence test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "-d interframe -p 16" "$test_path" >/dev/null 2>&1 \
            || ! grep -F -- "interframe:strategy=diffusion" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0045 must compare default interframe vs strategy=diffusion" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0045 must run in float32 mode" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0046_interframe_strategy_cli_stbn_alias_matches_hash_8bit_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing 8bit cli stbn alias equivalence test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "interframe:strategy=stbn -p 16" "$test_path" >/dev/null 2>&1 \
            || ! grep -F -- "interframe:strategy=stbn-hash -p 16" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0046 must compare CLI stbn and stbn-hash" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0046 must use CLI path without env override" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0046 must remain 8bit coverage" \
            >> "$missing"
        status=1
    fi
fi

test_path="$interframe_tests_dir/0047_interframe_strategy_cli_stbn_alias_matches_hash_float32_animated_gif.t"
if test ! -f "$test_path"; then
    echo "# tests/processing/dither/interframe: missing float32 cli stbn alias equivalence test" \
        >> "$missing"
    status=1
else
    if ! grep -F -- "interframe:strategy=stbn -p 16" "$test_path" >/dev/null 2>&1 \
            || ! grep -F -- "interframe:strategy=stbn-hash -p 16" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0047 must compare CLI stbn and stbn-hash" \
            >> "$missing"
        status=1
    fi
    if grep -F -- "SIXEL_DITHER_INTERFRAME_STRATEGY=stbn" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0047 must use CLI path without env override" \
            >> "$missing"
        status=1
    fi
    if ! grep -F -- "--precision=float32" "$test_path" >/dev/null 2>&1; then
        echo "# tests/processing/dither/interframe: 0047 must run in float32 mode" \
            >> "$missing"
        status=1
    fi
fi

if test "$status" -eq 0; then
    echo "ok 1 - interframe strategy tokens stay synchronized"
    exit 0
fi

echo "not ok 1 - interframe strategy tokens stay synchronized"
cat "$missing"
exit 1
