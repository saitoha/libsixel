#!/bin/sh
# Emit TAP for the single suboption registry and its mandatory metadata.

set -eux

echo "1..1"
set -v

src_root=$1
registry_file=$src_root/src/options-registry.c
help_file=$src_root/converters/img2sixel.c
man_file=$src_root/converters/img2sixel.1
duplicates=
dispatchers=

test -f "$registry_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# missing src/options-registry.c"
    exit 0
}

duplicates=$(find "$src_root/src" -maxdepth 1 -type f -name '*.c' \
    ! -name 'options-registry.c' -exec awk '
/static[[:space:]]+sixel_suboption_key_t[[:space:]]+const/ {
    print FILENAME ":" FNR ":" $0
}
' {} +)

test -z "$duplicates" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# suboption tables exist outside src/options-registry.c"
    printf '%s\n' "$duplicates" | sed 's/^/# /'
    exit 0
}

dispatchers=$(find "$src_root/src" -maxdepth 1 -type f -name '*.[ch]' \
    ! -name 'options.c' ! -name 'options.h' \
    ! -name 'loader-order-schema.c' -exec awk '
/resolved_key_name|resolved_value_text/ {
    print FILENAME ":" FNR ":" $0
}
' {} +)

test -z "$dispatchers" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# suboption application dispatch exists outside src/options.c"
    printf '%s\n' "$dispatchers" | sed 's/^/# /'
    exit 0
}

awk '
function fail(message) {
    print "# " message
    failed = 1
}
function inspect(row, fields, count, option_id, base, name, alias, env,
                 exact_key, shared_key, macro) {
    gsub(/[[:space:]]+/, " ", row)
    match(row, /SIXEL_REGISTRY_[A-Z0-9_]+/)
    macro = substr(row, RSTART, RLENGTH)
    sub(/^.*SIXEL_REGISTRY_[A-Z0-9_]+\(/, "", row)
    sub(/\),[[:space:]]*$/, "", row)
    count = split(row, fields, /,[[:space:]]*/)
    if (count < 7) {
        fail("malformed registry row: " row)
        return
    }
    option_id = fields[1]
    base = fields[2]
    name = fields[3]
    alias = fields[4]
    env = fields[5]
    gsub(/^"|"$/, "", name)
    if (name == "") {
        fail("registry row has no long suboption name")
    }
    if (alias !~ /^\047[A-Z]\047$/) {
        fail(option_id ":" name " needs one uppercase short name")
    }
    if (env !~ /^"[A-Z][A-Z0-9_]+"$/) {
        fail(option_id ":" name " needs a non-empty environment variable")
    }
    if (macro !~ /BOUND|ENCODER/) {
        fail(option_id ":" name " needs a typed target binding")
    }
    exact_key = option_id SUBSEP base SUBSEP alias
    if (seen_exact[exact_key]) {
        fail(option_id ":" base " reuses short name " alias)
    }
    seen_exact[exact_key] = 1
    shared_key = option_id SUBSEP alias
    if (base == "NULL") {
        if (seen_base_alias[shared_key]) {
            fail(option_id " common short name conflicts with " alias)
        }
        seen_common_alias[shared_key] = 1
    } else {
        if (seen_common_alias[shared_key]) {
            fail(option_id ":" base " conflicts with common " alias)
        }
        seen_base_alias[shared_key] = 1
    }
    rows += 1
}
BEGIN {
    in_registry = 0
    in_row = 0
    failed = 0
    rows = 0
    raw_initializer = 0
}
/g_suboptions\[\][[:space:]]*=[[:space:]]*\{/ {
    in_registry = 1
    next
}
in_registry && /^[[:space:]]*};/ {
    in_registry = 0
    next
}
in_registry && /SIXEL_REGISTRY_[A-Z0-9_]+\(/ {
    in_row = 1
    row = $0
    next
}
in_registry && in_row {
    row = row " " $0
}
in_registry && in_row && /\),[[:space:]]*$/ {
    inspect(row)
    in_row = 0
    row = ""
    next
}
in_registry && !in_row && /^[[:space:]]*\{/ {
    raw_initializer = 1
}
END {
    if (in_row) {
        fail("unterminated suboption registry row")
    }
    if (raw_initializer) {
        fail("raw suboption initializer bypasses registry macros")
    }
    if (rows == 0) {
        fail("suboption registry has no rows")
    }
    exit failed ? 1 : 0
}
' "$registry_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

test -f "$help_file" -a -f "$man_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# img2sixel help or manual source is missing"
    exit 0
}

# Both user-visible references must carry every long-to-short mapping.  The
# compact tables use name=A, while loader prose uses "short form Avalue".
awk -v registry_file="$registry_file" \
    -v help_file="$help_file" \
    -v man_file="$man_file" '
function fail(message) {
    print "# " message
    failed = 1
}
function inspect(row, fields, count, name, alias, key) {
    gsub(/[[:space:]]+/, " ", row)
    sub(/^.*SIXEL_REGISTRY_[A-Z0-9_]+\(/, "", row)
    sub(/\),[[:space:]]*$/, "", row)
    count = split(row, fields, /,[[:space:]]*/)
    name = fields[3]
    alias = fields[4]
    gsub(/^"|"$/, "", name)
    gsub(/^\047|\047$/, "", alias)
    key = name SUBSEP alias
    expected[key] = 1
    expected_name[key] = name
    expected_alias[key] = alias
}
function mapping_is_documented(text, name, alias, offset, position,
                               window, short_position, short_window) {
    if (index(text, name "=" alias) > 0) {
        return 1
    }
    offset = 1
    while (offset <= length(text)) {
        position = index(substr(text, offset), name)
        if (position == 0) {
            break
        }
        position += offset - 1
        window = substr(text, position, 240)
        if (index(window, ":" alias) > 0) {
            return 1
        }
        short_position = index(window, "shortform")
        if (short_position > 0) {
            short_window = substr(window, short_position, 24)
            if (index(short_window, alias) > 0) {
                return 1
            }
        }
        offset = position + length(name)
    }
    return 0
}
BEGIN {
    in_registry = 0
    in_row = 0
    failed = 0
}
FILENAME == registry_file {
    if ($0 ~ /g_suboptions\[\][[:space:]]*=[[:space:]]*\{/) {
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
        inspect(row)
        in_row = 0
        row = ""
    }
    next
}
{
    line = $0
    gsub(/[^[:alnum:]_=:.|\/-]+/, "", line)
    document[FILENAME] = document[FILENAME] line
}
END {
    for (key in expected) {
        name = expected_name[key]
        alias = expected_alias[key]
        if (!mapping_is_documented(document[help_file], name, alias)) {
            fail("img2sixel -H omits " name "=" alias)
        }
        if (!mapping_is_documented(document[man_file], name, alias)) {
            fail("img2sixel.1 omits " name "=" alias)
        }
    }
    exit failed ? 1 : 0
}
' "$registry_file" "$help_file" "$man_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

regression_dir=$src_root/tests/cli/options/regression
test -d "$regression_dir" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# missing per-suboption image regression tests"
    exit 0
}

awk -v registry_file="$registry_file" '
function fail(message) {
    print "# " message
    failed = 1
}
function inspect_registry(row, fields, count, option_id, name, alias,
                          environment, key, macro, binding) {
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
    expected_alias[key] = alias
    expected_environment[key] = environment
    if (macro ~ /ENCODER_MIRROR_CHOICE|ENCODER_INT_PAIR/) {
        binding = fields[count - 2] "|" fields[count - 1] "|" fields[count]
    } else if (macro ~ /ENCODER_DIRECT_CHOICE/) {
        binding = fields[count]
    } else if (macro ~ /ENCODER_/) {
        binding = fields[count - 1] "|" fields[count]
    } else if (macro ~ /BOUND_/) {
        binding = fields[count]
    } else {
        fail(option_id ":" name " has no typed binding macro")
    }
    expected_binding[key] = binding
    registry_rows += 1
}
BEGIN {
    in_registry = 0
    in_row = 0
    failed = 0
    registry_rows = 0
    test_rows = 0
}
FILENAME == registry_file {
    if ($0 ~ /g_suboptions\[\][[:space:]]*=[[:space:]]*\{/) {
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
FILENAME != registry_file {
    environment_first = ""
    environment_position = 0
    environment_needle = ""
    short_first = ""
    short_position = 0
    short_needle = ""
    regression_test[FILENAME] = 1
    key = test_key[FILENAME]
    if (key == "") {
        next
    }
    if (index($0, "LSQA_PATH") > 0) {
        has_lsqa[FILENAME] = 1
    }
    if (index($0, "cmp -s") > 0) {
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
    if (index($0, ">\"${short_output}\"") > 0) {
        has_short_redirection[FILENAME] = 1
    }
    if (index($0, ">\"${env_output}\"") > 0) {
        has_environment_redirection[FILENAME] = 1
    }
    if (index($0, "SIXEL_TRACE_TOPIC=suboption_contract") > 0) {
        contract_trace_count[FILENAME] += 1
    }
    if (index($0, "short_trace#*LSXSUB1|*key=" expected_name[key] \
            "|stored=1*") > 0) {
        has_short_contract[FILENAME] = 1
    }
    if (index($0, "env_trace#*LSXSUB1|*key=" expected_name[key] \
            "|stored=1*") > 0) {
        has_environment_contract[FILENAME] = 1
    }
    if (index($0, "ARTIFACT_ROOT") > 0 || index($0, "mkdir ") > 0 ||
            index($0, "-o \"${short_output}\"") > 0 ||
            index($0, "-o \"${env_output}\"") > 0) {
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
        key = test_key[file]
        if (test_binding[file] != expected_binding[key]) {
            fail(file " binding does not match registry field selection")
        }
        if (!has_lsqa[file]) {
            fail(file " does not validate decoded image quality")
        }
        if (!has_compare[file]) {
            fail(file " does not compare short and environment output")
        }
        if (!has_local_artifact_dir[file] ||
                !has_unique_short_output[file] ||
                !has_unique_environment_output[file]) {
            fail(file " does not isolate output in its local artifact dir")
        }
        if (!has_short_redirection[file] ||
                !has_environment_redirection[file] ||
                has_unsafe_artifact_handling[file]) {
            fail(file " can reuse or collide with stale image output")
        }
        if (contract_trace_count[file] < 2 ||
                !has_short_contract[file] ||
                !has_environment_contract[file]) {
            fail(file " does not verify both typed registry assignments")
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
' "$registry_file" "$regression_dir"/*.t || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

echo "ok 1 - suboptions use one complete registry"
exit 0
