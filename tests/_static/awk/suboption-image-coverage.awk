# SPDX-License-Identifier: MIT
# Verify the executable evidence attached to each registered suboption.
function fail(message) {
    print "# " message
    failed = 1
}
function inspect_registry(row, fields, count, option_id, name, alias,
                          environment, key, macro, binding, range_policy,
                          binding_value, binding_override) {
    gsub(/[[:space:]]+/, " ", row)
    match(row, /SIXEL_REGISTRY_[A-Z0-9_]+/)
    macro = substr(row, RSTART, RLENGTH)
    sub(/^.*SIXEL_REGISTRY_[A-Z0-9_]+\(/, "", row)
    sub(/^[[:space:]]*/, "", row)
    sub(/\),[[:space:]]*$/, "", row)
    count = split(row, fields, /,[[:space:]]*/)
    if (count < 7) {
        fail("malformed registry row in image coverage check: " row)
        return
    }
    option_id = fields[1]
    name = fields[3]
    alias = fields[4]
    environment = fields[5]
    gsub(/^"|"$/, "", name)
    gsub(/^\047|\047$/, "", alias)
    if (environment !~ /^"[A-Z][A-Z0-9_]+"$/) {
        fail("unresolved environment name in image coverage check: " \
             environment)
    }
    gsub(/^"|"$/, "", environment)
    if (environment == "") {
        fail(option_id ":" name " resolved an empty environment name")
    }
    key = fields[1] "|" fields[2] "|" name
    expected[key] = 1
    expected_option[key] = option_id
    expected_name[key] = name
    expected_base[key] = fields[2]
    sub(/^.*SIXEL_LOADER_INDEX_/, "", expected_base[key])
    expected_base[key] = tolower(expected_base[key])
    expected_alias[key] = alias
    expected_environment[key] = environment
    range_policy = ""
    if (macro ~ /CHOICE_ENV_PARSE_SIGNED_LONG/) {
        range_policy = "parse-signed-choice"
    } else if (macro == "SIXEL_REGISTRY_RUNTIME_CHOICE") {
        range_policy = "parse-signed-choice-prefix"
    } else if (macro == "SIXEL_REGISTRY_RUNTIME_SIZE") {
        range_policy = "clamp-size-width"
    } else if (macro == "SIXEL_REGISTRY_RUNTIME_UINT") {
        range_policy = "parse-signed-long"
    } else if (macro == "SIXEL_REGISTRY_RUNTIME_INT") {
        range_policy = "clamp-both"
    } else if (macro == "SIXEL_REGISTRY_DIAGNOSTICS_INT") {
        range_policy = "clamp-both"
    } else if (macro ~ /SCALED_U8_ENV_CLAMP|DOUBLE_ENV_CLAMP/) {
        range_policy = "clamp-both"
    } else if (macro ~ /UINT_ENV_CLAMP_SIGNED/) {
        range_policy = "clamp-signed-uint"
    } else if (macro ~ /UINT_ENV_CLAMP_UNSIGNED/) {
        range_policy = "clamp-unsigned-uint"
    } else if (macro ~ /UINT_ENV_CLAMP_POSITIVE/) {
        range_policy = "clamp-positive-uint"
    } else if (macro ~ /UINT_ENV_CLAMP_MAXIMUM/) {
        range_policy = "clamp-maximum"
    } else if (macro ~ /UINT_ENV_REJECT_SIGNED/) {
        range_policy = "parse-signed-long"
    } else if (macro ~ /UINT_ENV_REJECT_UNSIGNED_LONG/) {
        range_policy = "parse-unsigned-long"
        if ((fields[8] + 0.0) == 0.0 || (fields[10] + 0) == 1) {
            expected_unsigned_long_sign[key] = 1
        }
    } else if (macro ~ /ENCODER_POSITIVE_SIZE/) {
        range_policy = "parse-positive-size"
    } else if (macro ~ /ENCODER_SIZE|DECODER_SIZE/) {
        range_policy = "saturate-unsigned-long"
    }
    expected_range_policy[key] = range_policy
    if (option_id == "SIXEL_OPTION_SCHEMA_DIFFUSION" &&
            ((fields[2] == "NULL" &&
              (name == "scan" || name == "band_overwrap" ||
               name == "band_width" || name == "threads_max" ||
               name == "pin_threads" || name == "perturb" ||
               name == "perturb_seed" || name == "deadzone")) ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_SIERRA/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_A_DITHER/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_X_DITHER/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_INTERFRAME/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_STBN/)) {
        expected_dither_contract[key] = 1
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_CLAMP_POLICY") {
        expected_dither_contract[key] = 1
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_LUT_POLICY") {
        expected_lookup_contract[key] = 1
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_MERGE_POLICY" ||
            option_id == "SIXEL_OPTION_SCHEMA_COVER_POLICY" ||
            option_id == "SIXEL_OPTION_SCHEMA_SNAP_POLICY" ||
            (option_id == "SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL" &&
             name == "sample_target")) {
        expected_palette_contract[key] = 1
    }
    if (macro ~ /MULTI/) {
        expected_multi_base_contract[key] = 1
        if (option_id == "SIXEL_OPTION_SCHEMA_UPDATE_POLICY") {
            expected_presentation_stop[key] = name == "stop_threshold" \
                ? "converged" : name
        } else if (option_id == "SIXEL_OPTION_SCHEMA_DIFFUSION") {
            expected_dither_contract[key] = 1
        } else if (option_id != "SIXEL_OPTION_SCHEMA_LUT_POLICY") {
            fail(option_id ":" name " lacks a multi-base consumer check")
        }
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_GPU_POLICY" &&
            name == "palette_threshold") {
        expected_dither_contract[key] = 1
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_GPU_POLICY" &&
            name == "dequant_threshold") {
        expected_gpu_contract[key] = 1
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_RUNTIME_POLICY") {
        if (name == "colorspace_min") {
            expected_runtime_contract[key] = "trace=colorspace_min=257"
        } else if (name == "sample_min") {
            expected_runtime_contract[key] = "trace=sample_min=257"
        } else if (name == "parallel_factor") {
            expected_runtime_contract[key] = \
                "runner=scale/0001_parallel_factor_environment|mode=cli"
        } else if (name == "parallel_skew") {
            expected_runtime_contract[key] = \
                "runner=decoder/0024_decoder_parallel_skew_environment|mode=cli"
        } else if (name == "resize_precision") {
            expected_runtime_contract[key] = "trace=resize_precision=3"
        } else if (name == "scale_min_bytes") {
            expected_runtime_contract[key] = \
                "runner=scale/0002_parallel_min_bytes_environment|mode=cli,negative"
        }
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_DIAGNOSTICS" &&
            name == "log_lines") {
        expected_timeline_contract[key] = \
            "line_events=1|line_stride=3"
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_DIAGNOSTICS" &&
            name == "abort_trace") {
        expected_abort_contract[key] = "enabled=0|installed=0"
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_CLIPBOARD_POLICY" &&
            name == "directory") {
        expected_clipboard_contract[key] = "backend=file|directory=1"
    }
    if (option_id == "IMG2SIXEL_OPTION_SCHEMA_COMPLETION_POLICY") {
        expected_completion_contract[key] = \
            "key=" name "|configured=1|used=1"
    }
    binding_value = ""
    binding_override = ""
    if (macro ~ /ENCODER_SIZE|DECODER_SIZE|LOADER_SIZE_ENV_ERROR/) {
        binding = fields[count - 1] "|" fields[count]
        binding_value = fields[count - 1]
        binding_override = fields[count]
    } else if (macro ~ /ENCODER_MIRROR_CHOICE/) {
        binding = fields[count - 2] "|" fields[count - 1] "|" fields[count]
        binding_value = fields[count - 2]
        binding_override = fields[count - 1]
    } else if (macro ~ /ENCODER_INT_PAIR/) {
        binding = fields[count - 2] "|" fields[count - 1] "|" fields[count]
        binding_value = fields[count - 2]
        binding_override = fields[count]
    } else if (macro ~ /ENCODER_DIRECT_CHOICE|OUTPUT_COLOR_CHOICE/) {
        binding = fields[count]
        binding_value = fields[count]
    } else if (macro ~ /ENCODER_/) {
        binding = fields[count - 1] "|" fields[count]
        binding_value = fields[count - 1]
        binding_override = fields[count]
    } else if (macro ~ /DEQUANTIZE_|LOADER_/) {
        binding = fields[count]
    } else if (macro ~ /RUNTIME_|DIAGNOSTICS_|CLIPBOARD_|COMPLETION_|COLOR_STRING/) {
        binding = fields[count - 1] "|" fields[count]
        binding_value = fields[count - 1]
        binding_override = fields[count]
    } else {
        fail(option_id ":" name " has no typed binding macro")
    }
    if (binding_value ~ /_override$/) {
        fail(option_id ":" name " binds a control field as its value")
    }
    if (binding_override != "" && binding_override !~ /_override$/) {
        fail(option_id ":" name " binds a value field as its override")
    }
    expected_binding[key] = binding
    expected_trace_binding[key] = binding
    gsub(/\|/, ",", expected_trace_binding[key])
    registry_rows += 1
}
BEGIN {
    in_registry = 0
    in_row = 0
    failed = 0
    registry_rows = 0
    test_rows = 0
}
FILENAME == registry_file || FILENAME == completion_registry_file {
    if ($0 ~ /(g_suboptions|g_img2sixel_completion_policy_keys)\[\][[:space:]]*=[[:space:]]*\{/) {
        in_registry = 1
        next
    }
    if (in_registry && $0 ~ /^[[:space:]]*};/) {
        in_registry = 0
        next
    }
    if (in_registry && $0 ~ /SIXEL_REGISTRY_[A-Z0-9_]+\(/) {
        in_row = 1
        row = $0
        next
    }
    if (in_registry && in_row) {
        row = row " " $0
    }
    if (in_registry && in_row && $0 ~ /\),[[:space:]]*$/) {
        inspect_registry(row)
        in_row = 0
        row = ""
    }
    next
}
/^# Top-level option regression$/ {
    if (top_level_test[FILENAME]) {
        fail(FILENAME " contains more than one top-level marker")
    }
    top_level_test[FILENAME] = 1
    next
}
/^# Registry row: / {
    key = $0
    sub(/^# Registry row: /, "", key)
    gsub(/[[:space:]]+/, " ", key)
    if (test_key[FILENAME] != "") {
        fail(FILENAME " contains more than one registry row marker")
    }
    if (covered[key]) {
        fail("duplicate image regression coverage for " key)
    }
    test_key[FILENAME] = key
    covered[key] = FILENAME
    test_rows += 1
    next
}
/^# Registry binding: / {
    binding = $0
    sub(/^# Registry binding: /, "", binding)
    gsub(/[[:space:]]+/, " ", binding)
    if (test_binding[FILENAME] != "") {
        fail(FILENAME " contains more than one registry binding marker")
    }
    test_binding[FILENAME] = binding
    next
}
/^# Dither contract: / {
    dither_contract = $0
    sub(/^# Dither contract: /, "", dither_contract)
    gsub(/[[:space:]]+/, " ", dither_contract)
    if (test_dither_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one dither contract marker")
    }
    test_dither_contract[FILENAME] = dither_contract
    next
}
/^# Lookup contract: / {
    lookup_contract = $0
    sub(/^# Lookup contract: /, "", lookup_contract)
    gsub(/[[:space:]]+/, " ", lookup_contract)
    if (test_lookup_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one lookup contract marker")
    }
    test_lookup_contract[FILENAME] = lookup_contract
    next
}
/^# GPU contract: / {
    gpu_contract = $0
    sub(/^# GPU contract: /, "", gpu_contract)
    gsub(/[[:space:]]+/, " ", gpu_contract)
    if (test_gpu_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one GPU contract marker")
    }
    test_gpu_contract[FILENAME] = gpu_contract
    next
}
/^# Palette contract: / {
    palette_contract = $0
    sub(/^# Palette contract: /, "", palette_contract)
    gsub(/[[:space:]]+/, " ", palette_contract)
    if (test_palette_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one palette contract marker")
    }
    test_palette_contract[FILENAME] = palette_contract
    next
}
/^# Runtime contract: / {
    runtime_contract = $0
    sub(/^# Runtime contract: /, "", runtime_contract)
    gsub(/[[:space:]]+/, " ", runtime_contract)
    if (test_runtime_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one runtime contract marker")
    }
    test_runtime_contract[FILENAME] = runtime_contract
    next
}
/^# Timeline contract: / {
    timeline_contract = $0
    sub(/^# Timeline contract: /, "", timeline_contract)
    gsub(/[[:space:]]+/, " ", timeline_contract)
    if (test_timeline_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one timeline contract marker")
    }
    test_timeline_contract[FILENAME] = timeline_contract
    next
}
/^# Abort trace contract: / {
    abort_contract = $0
    sub(/^# Abort trace contract: /, "", abort_contract)
    gsub(/[[:space:]]+/, " ", abort_contract)
    if (test_abort_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one abort trace contract marker")
    }
    test_abort_contract[FILENAME] = abort_contract
    next
}
/^# Clipboard contract: / {
    clipboard_contract = $0
    sub(/^# Clipboard contract: /, "", clipboard_contract)
    gsub(/[[:space:]]+/, " ", clipboard_contract)
    if (test_clipboard_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one clipboard contract marker")
    }
    test_clipboard_contract[FILENAME] = clipboard_contract
    next
}
/^# Completion contract: / {
    completion_contract = $0
    sub(/^# Completion contract: /, "", completion_contract)
    gsub(/[[:space:]]+/, " ", completion_contract)
    if (test_completion_contract[FILENAME] != "") {
        fail(FILENAME " contains more than one completion contract marker")
    }
    test_completion_contract[FILENAME] = completion_contract
    next
}
/^# Environment range: / {
    range_policy = $0
    sub(/^# Environment range: /, "", range_policy)
    gsub(/[[:space:]]+/, " ", range_policy)
    if (test_range_policy[FILENAME] != "") {
        fail(FILENAME " contains more than one environment range marker")
    }
    test_range_policy[FILENAME] = range_policy
    next
}
/^# Image assertion: option-values-and-output-equivalence$/ {
    value_equivalence[FILENAME] = 1
}
/^# Exact image reference: literal-pnm$/ {
    exact_image_reference[FILENAME] = 1
    next
}
FILENAME != registry_file {
    regression_test[FILENAME] = 1
    if ($0 ~ /^[[:space:]]*#/) next
    # Escaping a pipe in a shell case pattern does not change the
    # literal trace field separator. Normalize only this equivalent spelling.
    gsub(/\\[|]/, "|", $0)
    environment_first = ""
    environment_position = 0
    environment_needle = ""
    short_first = ""
    short_position = 0
    short_needle = ""
    regression_test[FILENAME] = 1
    test_source[FILENAME] = test_source[FILENAME] " " $0
    key = test_key[FILENAME]
    if (key == "") {
        next
    }
    if (index($0, "LSQA_PATH") > 0) {
        has_lsqa[FILENAME] = 1
    }
    if (index($0, "cmp -s") > 0 &&
            index($0, "short_output") > 0 &&
            index($0, "env_output") > 0) {
        has_compare[FILENAME] = 1
    }
    if (index($0, "artifact_dir=\"${ARTIFACT_LOCAL_DIR}\"") > 0) {
        has_local_artifact_dir[FILENAME] = 1
    }
    if (index($0, "short-$$.") > 0) {
        has_unique_short_output[FILENAME] = 1
    }
    if (index($0, "env-$$.") > 0) {
        has_unique_environment_output[FILENAME] = 1
    }
    # img2sixel -o opens with O_TRUNC and preserves binary bytes on RMS.
    # Both forms replace an existing capture; append is rejected below.
    if (index($0, ">\"${short_output}\"") > 0 ||
            index($0, "-o \"${short_output}\"") > 0) {
        has_short_capture[FILENAME] = 1
    }
    if (index($0, ">\"${env_output}\"") > 0 ||
            index($0, "-o \"${env_output}\"") > 0) {
        has_environment_capture[FILENAME] = 1
    }
    # Complete CMS records are stronger than the older wildcard probes.
    # Retain the extraction, equality and actual-apply requirements, tied to
    # the corresponding command output rather than counting string mentions.
    if (index($0, "trace=$(") > 0) {
        cms_phase[FILENAME] = ""
        cms_record[FILENAME] = 0
        cms_compare[FILENAME] = 0
        cms_apply[FILENAME] = 0
        cms_clip[FILENAME] = 0
        cms_engine[FILENAME] = 0
    }
    if (index($0, ">\"${short_output}\"") > 0 ||
            index($0, "-o \"${short_output}\"") > 0) {
        cms_phase[FILENAME] = "short"
    }
    if (index($0, ">\"${env_output}\"") > 0 ||
            index($0, "-o \"${env_output}\"") > 0) {
        cms_phase[FILENAME] = "env"
    }
    phase = cms_phase[FILENAME]
    if (expected_name[key] == "cms_engine" && phase != "") {
        if (index($0, "record=${trace##*\"LSXSUB1|schema=9|base=" \
                expected_base[key] "|key=cms_engine|\"}") > 0) {
            cms_record[FILENAME] = 1
        }
        if (cms_record[FILENAME] && index($0, \
                "test \"${record%%[![:print:]]*}\" = ") > 0) {
            cms_compare[FILENAME] = 1
        }
        if (cms_compare[FILENAME] && index($0, \
                "\"stored=1|binding=" expected_trace_binding[key] \
                "|value=2\" || {") > 0) {
            if (phase == "short") has_short_contract[FILENAME] = 1
            if (phase == "env") has_environment_contract[FILENAME] = 1
        }
        if (index($0, "record=${trace##*\"LSXCMS2|event=apply|\"}") > 0) {
            cms_apply[FILENAME] = 1
        }
        if (cms_apply[FILENAME] && index($0, \
                "record=${record%%[![:print:]]*}") > 0) {
            cms_clip[FILENAME] = 1
        }
        if (cms_clip[FILENAME] && index($0, \
                "case \"${record}\" in engine=builtin|*) true ;; *) false ;; esac &&") > 0) {
            cms_engine[FILENAME] = 1
        }
        if (cms_engine[FILENAME] && index($0, \
                "case \"${record}\" in *|ok=1) true ;; *) false ;; esac; } || {") > 0) {
            has_cms_consumer[FILENAME, phase] = 1
        }
    }
    if (index($0, "SIXEL_TRACE_TOPIC=suboption_contract") > 0) {
        contract_trace_count[FILENAME] += 1
    }
    if (index($0, "case \"${short_trace}\" in *LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key]) > 0) {
        has_short_contract[FILENAME] = 1
    }
    if (index($0, "case \"${env_trace}\" in *LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key]) > 0) {
        has_environment_contract[FILENAME] = 1
    }
    if (index($0, "case \"${short_trace}\" in *LSXCMP1|*key=" expected_name[key] \
            "|configured=1|override=1|used=1*") > 0) {
        has_short_completion_contract[FILENAME] = 1
    }
    if (index($0, "case \"${env_trace}\" in *LSXCMP1|*key=" expected_name[key] \
            "|configured=1|override=0|used=1*") > 0) {
        has_environment_completion_contract[FILENAME] = 1
    }
    if (index($0, "case \"${short_trace}\" in *LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key] \
            "|value=") > 0) {
        has_short_value_contract[FILENAME] = 1
    }
    if (index($0, "case \"${env_trace}\" in *LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key] \
            "|value=") > 0) {
        has_environment_value_contract[FILENAME] = 1
    }
    if (test_dither_contract[FILENAME] != "" &&
            index($0, "LSXDTH1|*" test_dither_contract[FILENAME] "*") > 0) {
        has_dither_contract[FILENAME] = 1
    }
    if (test_lookup_contract[FILENAME] != "" &&
            (index($0, "LSXDTH1|*" test_lookup_contract[FILENAME] "*") > 0 ||
             index($0, "LSXLUT1|*" test_lookup_contract[FILENAME] "*") > 0 ||
             index($0, "LSXFHD1|*" test_lookup_contract[FILENAME] "*") > 0)) {
        has_lookup_contract[FILENAME] = 1
        lookup_contract_count[FILENAME] += 1
    }
    if (test_gpu_contract[FILENAME] != "" &&
            index($0, "LSXGPU1|*" test_gpu_contract[FILENAME] "*") > 0) {
        has_gpu_contract[FILENAME] = 1
    }
    if (test_palette_contract[FILENAME] != "" &&
            (index($0, "LSXMRG1|*" test_palette_contract[FILENAME] "*") > 0 ||
             index($0, "LSXCOV1|*" test_palette_contract[FILENAME] "*") > 0 ||
             index($0, "LSXSNP1|*" test_palette_contract[FILENAME] "*") > 0 ||
             index($0, "LSXSMP1|*" test_palette_contract[FILENAME] "*") > 0)) {
        has_palette_contract[FILENAME] = 1
    }
    if (index($0, "case \"${range_trace}\" in *LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key] \
            "|value=") > 0) {
        has_range_value_contract[FILENAME] = 1
    }
    if (index($0, "case \"${range_trace}\" in *LSXSUB1|*key=" expected_name[key] \
            "|stored=1") > 0 && index($0, \
            ") false ;; *) true ;; esac") > 0) {
        has_range_reject_contract[FILENAME] = 1
    }
    if (index($0, "case \"${width_trace}\" in *LSXSUB1|*key=" expected_name[key] \
            "|stored=1") > 0 && index($0, \
            ") false ;; *) true ;; esac") > 0) {
        has_width_reject_contract[FILENAME] = 1
    }
    lazy_artifact_mkdir = index($0,
        "mkdir -p \"${ARTIFACT_LOCAL_DIR}\"") > 0
    clipboard_output_mkdir = expected_option[key] == \
        "SIXEL_OPTION_SCHEMA_CLIPBOARD_POLICY" && index($0, \
        "mkdir -p \"${short_dir}\" \"${env_dir}\"") > 0
    if (index($0, "ARTIFACT_ROOT") > 0 ||
            (index($0, "mkdir ") > 0 && !lazy_artifact_mkdir &&
             !clipboard_output_mkdir) ||
            index($0, ">>\"${short_output}\"") > 0 ||
            index($0, ">>\"${env_output}\"") > 0) {
        has_unsafe_artifact_handling[FILENAME] = 1
    }
    environment_needle = "--env \"" expected_environment[key] "="
    environment_position = index($0, environment_needle)
    if (environment_position > 0) {
        environment_value_index = environment_position + \
            length(environment_needle)
        environment_first = substr($0, environment_value_index, 1)
        if (environment_first != "" && environment_first != "\"") {
            has_environment[FILENAME] = 1
        }
    }
    short_needle = ":" expected_alias[key]
    short_position = index($0, short_needle)
    if (short_position > 0) {
        short_value_index = short_position + length(short_needle)
        short_first = substr($0, short_value_index, 1)
        if (short_first != "" && short_first != "\"" &&
                short_first != "," && short_first != ":") {
            has_short[FILENAME] = 1
        }
    }
    if (expected_option[key] == "SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL" &&
            $0 ~ /^[[:space:]]+/ && index($0, "-p ") > 0) {
        palette_limit_count[FILENAME] += 1
    }
}
END {
    for (file in regression_test) {
        if (top_level_test[file]) {
            if (test_key[file] != "") {
                fail(file " mixes top-level and suboption markers")
            }
            continue
        }
        if (test_key[file] == "") {
            fail(file " has no registry row marker")
        }
    }
    for (key in expected) {
        if (!covered[key]) {
            fail("missing image regression test for " key)
        }
    }
    for (key in covered) {
        if (!expected[key]) {
            fail("image regression test has stale registry row: " key)
        }
    }
    for (file in test_key) {
        if (top_level_test[file]) {
            continue
        }
        key = test_key[file]
        is_completion = expected_option[key] == \
            "IMG2SIXEL_OPTION_SCHEMA_COMPLETION_POLICY"
        if (test_binding[file] != expected_binding[key]) {
            fail(file " binding does not match registry field selection")
        }
        # Threaded shared-cache output can differ while preserving the
        # stated quality floor. Require both actual output arguments and a
        # checked LSQA status, not just the tool name or a comment marker.
        source = test_source[file]
        gsub(/\\[[:space:]]+/, " ", source)
        gsub(/[[:space:]]+/, " ", source)
        if (index(source, "equivalence_error=$(set +xv; " \
                "${SIXEL_RUNTIME-} \"${LSQA_PATH}\" " \
                "-m MS-SSIM -b \"MS-SSIM:0.98\" \"${short_output}\" " \
                "\"${env_output}\" 2>&1) || equivalence_status=$?") > 0 &&
                index(source, "test \"${equivalence_status:-0}\" -eq 0 || {") > 0) {
            has_compare[file] = 1
            # Pairwise agreement cannot replace an independent image
            # reference: both option paths could share the same defect.
            if (index(source, "lsqa_error=$(set +xv; " \
                    "${SIXEL_RUNTIME-} \"${LSQA_PATH}\" " \
                    "-m MS-SSIM -b \"MS-SSIM:0.98\" \"${reference_image}\" " \
                    "\"${short_output}\" 2>&1) || lsqa_status=$?") == 0 ||
                    index(source, "test \"${lsqa_status:-0}\" -eq 0 || {") == 0) {
                fail(file " lacks a checked independent image reference")
            }
        }
        has_exact_reference = exact_image_reference[file] &&
            index(source, "printf ") > 0 &&
            index(source, "P3\\n2 1\\n255\\n255 0 0 0 0 0\\n") > 0 &&
            index(source, ">\"${reference_output}\"") > 0 &&
            index(source, "cmp -s \"${reference_output}\" \"${short_output}\" || {") > 0
        if (expected_name[key] == "cms_engine" &&
                (!has_cms_consumer[file, "short"] ||
                 !has_cms_consumer[file, "env"])) {
            fail(file " does not verify both actual CMS apply records")
        }
        # Routing tests own typed values, consumer selection and exact output
        # equivalence. They make no independent pixel-quality claim. Require
        # nonempty output as well as the checked comparison and the ordinary
        # assignment/consumer checks below; a marker alone is not evidence.
        has_value_equivalence = value_equivalence[file] && has_compare[file] &&
            index(source, "test -s \"${short_output}\" || {") > 0
        if (value_equivalence[file] && !has_value_equivalence) {
            fail(file " lacks checked nonempty output equivalence")
        }
        has_pixel_values = index(source, "${SIXEL_RUNTIME-} " \
            "\"${TEST_RUNNER_PATH}\" loader/image_values \"$reference\" " \
            "\"$short_output\" 'builtin:Enone!' 2.3 || {") > 0 &&
            index(source, "tests/data/color-numeric/cli-rgb-reference.png") > 0
        if (!is_completion && !has_lsqa[file] && !has_exact_reference &&
                !has_value_equivalence && !has_pixel_values) {
            fail(file " lacks a pixel reference or option-value observation")
        }
        if (!is_completion && !has_compare[file]) {
            fail(file " does not compare short and environment output")
        }
        if (!is_completion && (!has_local_artifact_dir[file] ||
                !has_unique_short_output[file] ||
                !has_unique_environment_output[file])) {
            fail(file " does not isolate output in its local artifact dir")
        }
        if (!is_completion && (!has_short_capture[file] ||
                !has_environment_capture[file] ||
                has_unsafe_artifact_handling[file])) {
            fail(file " can reuse or collide with stale image output")
        }
        if (!is_completion && (contract_trace_count[file] < 2 ||
                !has_short_contract[file] ||
                !has_environment_contract[file])) {
            fail(file " does not verify both typed registry assignments")
        }
        if (is_completion &&
                (contract_trace_count[file] < 2 ||
                 !has_short_contract[file])) {
            fail(file " does not verify its typed CLI registry assignment")
        }
        if (is_completion &&
                test_completion_contract[file] != \
                    expected_completion_contract[key]) {
            fail(file " does not identify its completion consumer setting")
        }
        if (is_completion &&
                (!has_short_completion_contract[file] ||
                 !has_environment_completion_contract[file])) {
            fail(file " does not verify both completion consumer paths")
        }
        if (!is_completion && test_completion_contract[file] != "") {
            fail(file " has an unexpected completion contract marker")
        }
        if (expected_range_policy[key] != "" &&
                (!has_short_value_contract[file] ||
                 !has_environment_value_contract[file])) {
            fail(file " does not verify the environment clamp endpoint")
        }
        if (test_range_policy[file] != expected_range_policy[key]) {
            fail(file " does not identify its exact environment range policy")
        }
        if (expected_range_policy[key] != "" &&
                expected_range_policy[key] != "parse-unsigned-long" &&
                !has_range_value_contract[file] &&
                !has_range_reject_contract[file]) {
            fail(file " does not verify the opposite environment endpoint")
        }
        if ((expected_range_policy[key] == "clamp-positive-uint" ||
             expected_range_policy[key] == "parse-positive-size" ||
             expected_range_policy[key] == "parse-unsigned-long") &&
                !has_width_reject_contract[file]) {
            fail(file " does not verify unsigned-width rejection")
        }
        if (expected_unsigned_long_sign[key] &&
                !has_range_value_contract[file]) {
            fail(file " does not verify unsigned-long signed-zero parsing")
        }
        if (expected_dither_contract[key] &&
                (test_dither_contract[file] == "" ||
                 !has_dither_contract[file])) {
            fail(file " does not verify its effective dither setting")
        }
        if (!expected_dither_contract[key] &&
                test_dither_contract[file] != "") {
            fail(file " has an unexpected dither contract marker")
        }
        if (expected_lookup_contract[key] &&
                (test_lookup_contract[file] == "" ||
                 !has_lookup_contract[file])) {
            fail(file " does not verify its effective lookup setting")
        }
        if (expected_multi_base_contract[key] &&
                expected_lookup_contract[key] &&
                lookup_contract_count[file] < 2) {
            fail(file " does not verify every multi-base lookup consumer")
        }
        # A numeric repair option is shared by still and delta presentation.
        # Require checked stop outcomes from both CLI and environment routes
        # for each base, rather than accepting assignment traces alone.
        if (expected_presentation_stop[key] != "") {
            for (route = 1; route <= 4; ++route) {
                trace = route == 1 ? "short" : route == 2 ? "env" \
                    : route == 3 ? "delta_short" : "delta_env"
                outcome = expected_presentation_stop[key] " passes=1\"*"
                # Time need not advance between two calls. The CLI checks
                # its consumed budget with a deterministic one-pass cap;
                # injected-clock C tests own actual deadline behavior.
                if (expected_presentation_stop[key] == "max_seconds") {
                    outcome = "max_passes passes=1\"*\"max_seconds=1e-300\"*"
                }
                needle = "case \"${" trace "_trace}\" in *\"progressive: stop=" \
                    outcome ") true ;; *) false ;; esac || {"
                if (index(source, needle) == 0) {
                    fail(file " lacks the " trace " presentation outcome")
                }
            }
            if (index(source, "-Z progressive") == 0 ||
                    index(source, "-Z delta:P1") == 0) {
                fail(file " does not exercise both presentation bases")
            }
        }
        if (!expected_lookup_contract[key] &&
                test_lookup_contract[file] != "") {
            fail(file " has an unexpected lookup contract marker")
        }
        if (expected_gpu_contract[key] &&
                (test_gpu_contract[file] == "" ||
                 !has_gpu_contract[file])) {
            fail(file " does not verify its effective GPU setting")
        }
        if (!expected_gpu_contract[key] &&
                test_gpu_contract[file] != "") {
            fail(file " has an unexpected GPU contract marker")
        }
        if (expected_palette_contract[key] &&
                (test_palette_contract[file] == "" ||
                 !has_palette_contract[file])) {
            fail(file " does not verify its effective palette setting")
        }
        if (!expected_palette_contract[key] &&
                test_palette_contract[file] != "") {
            fail(file " has an unexpected palette contract marker")
        }
        if (expected_runtime_contract[key] != "" &&
                test_runtime_contract[file] != \
                    expected_runtime_contract[key]) {
            fail(file " does not identify its effective runtime setting")
        }
        if (expected_runtime_contract[key] ~ /^trace=/) {
            runtime_trace = expected_runtime_contract[key]
            sub(/^trace=/, "", runtime_trace)
            if (index(test_source[file],
                      "LSXRT1|" runtime_trace "*") == 0) {
                fail(file " does not verify its runtime trace")
            }
        }
        if (expected_runtime_contract[key] ~ /^runner=/) {
            runtime_runner = expected_runtime_contract[key]
            sub(/^runner=/, "", runtime_runner)
            sub(/\|mode=.*$/, "", runtime_runner)
            if (index(test_source[file], "TEST_RUNNER_PATH") == 0 ||
                    index(test_source[file], runtime_runner) == 0 ||
                    index(test_source[file], " cli") == 0) {
                fail(file " does not verify its runtime consumer")
            }
            if (expected_runtime_contract[key] ~ /,negative$/ &&
                    index(test_source[file], " negative") == 0) {
                fail(file " does not verify signed unsigned parsing")
            }
        }
        if (expected_runtime_contract[key] == "" &&
                test_runtime_contract[file] != "") {
            fail(file " has an unexpected runtime contract marker")
        }
        if (expected_timeline_contract[key] != "" &&
                test_timeline_contract[file] != expected_timeline_contract[key]) {
            fail(file " does not identify its effective timeline setting")
        }
        if (expected_timeline_contract[key] != "" &&
                index(test_source[file],
                      "LSXTLN1|*" expected_timeline_contract[key] "*") == 0) {
            fail(file " does not verify its timeline trace")
        }
        if (expected_timeline_contract[key] == "" &&
                test_timeline_contract[file] != "") {
            fail(file " has an unexpected timeline contract marker")
        }
        if (expected_abort_contract[key] != "" &&
                test_abort_contract[file] != expected_abort_contract[key]) {
            fail(file " does not identify its effective abort trace setting")
        }
        if (expected_abort_contract[key] != "" &&
                index(test_source[file],
                      "LSXABT1|*" expected_abort_contract[key] "*") == 0) {
            fail(file " does not verify its abort trace consumer")
        }
        if (expected_abort_contract[key] == "" &&
                test_abort_contract[file] != "") {
            fail(file " has an unexpected abort trace contract marker")
        }
        if (expected_clipboard_contract[key] != "" &&
                test_clipboard_contract[file] != expected_clipboard_contract[key]) {
            fail(file " does not identify its effective clipboard setting")
        }
        if (expected_clipboard_contract[key] != "" &&
                index(test_source[file],
                      "LSXCLP1|*" expected_clipboard_contract[key] "*") == 0) {
            fail(file " does not verify its clipboard consumer")
        }
        if (expected_clipboard_contract[key] == "" &&
                test_clipboard_contract[file] != "") {
            fail(file " has an unexpected clipboard contract marker")
        }
        if (!has_environment[file]) {
            fail(file " does not exercise the registered environment name")
        }
        if (!has_short[file]) {
            fail(file " does not exercise the registered short name")
        }
        if (expected_option[key] == "SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL" &&
                palette_limit_count[file] < 2) {
            fail(file " does not force both quantization paths")
        }
    }
    if (registry_rows != test_rows) {
        fail("registry/image test count mismatch: " registry_rows "/" \
             test_rows)
    }
    exit failed ? 1 : 0
}
