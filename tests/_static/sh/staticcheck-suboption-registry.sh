#!/bin/sh
# Emit TAP for the typed suboption registries and their mandatory metadata.
# Policy: docs/cli/design-policy.md
# Policy: docs/cli/abort-trace/README.md
# Policy: docs/cli/suboptions.md

set -eux

echo "1..1"
set -v

src_root=$1
registry_file=$src_root/src/options-registry.c
completion_registry_file=$src_root/converters/completion_utils.c
help_file=$src_root/converters/img2sixel.c
man_file=$src_root/converters/img2sixel.1
decoder_help_file=$src_root/converters/sixel2png.c
decoder_man_file=$src_root/converters/sixel2png.1
public_header=$src_root/include/sixel.h.in
duplicates=
dispatchers=

test -f "$registry_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# missing src/options-registry.c"
    exit 0
}

test -f "$completion_registry_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# missing converters/completion_utils.c"
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

# Top-level schemas own their scope, short/long spelling, argument shape, and
# default policy.  Keep this closed macro table even while getopt remains a
# separately checked compatibility surface.
awk '
function fail(message) {
    print "# " message
    failed = 1
}
function macro_is_approved(macro) {
    return macro == "SIXEL_REGISTRY_OPTION_SCHEMA" ||
        macro == "SIXEL_REGISTRY_RUNTIME_OPTION_SCHEMA" ||
        macro == "SIXEL_REGISTRY_DIAGNOSTICS_OPTION_SCHEMA" ||
        macro == "SIXEL_REGISTRY_CLIPBOARD_OPTION_SCHEMA" ||
        macro == "SIXEL_REGISTRY_SCALAR_BOOLEAN" ||
        macro == "SIXEL_REGISTRY_SCALAR_CHOICE" ||
        macro == "SIXEL_REGISTRY_SCALAR_CHOICE_ENV" ||
        macro == "SIXEL_REGISTRY_SCALAR_INT" ||
        macro == "SIXEL_REGISTRY_SCALAR_UINT" ||
        macro == "SIXEL_REGISTRY_SCALAR_STRING"
}
function expected_field_count(macro) {
    if (macro == "SIXEL_REGISTRY_OPTION_SCHEMA") return 9
    if (macro == "SIXEL_REGISTRY_RUNTIME_OPTION_SCHEMA") return 7
    if (macro == "SIXEL_REGISTRY_DIAGNOSTICS_OPTION_SCHEMA") return 7
    if (macro == "SIXEL_REGISTRY_CLIPBOARD_OPTION_SCHEMA") return 8
    if (macro == "SIXEL_REGISTRY_SCALAR_BOOLEAN") return 7
    if (macro == "SIXEL_REGISTRY_SCALAR_CHOICE") return 10
    if (macro == "SIXEL_REGISTRY_SCALAR_CHOICE_ENV") return 11
    if (macro == "SIXEL_REGISTRY_SCALAR_INT") return 22
    if (macro == "SIXEL_REGISTRY_SCALAR_UINT") return 15
    if (macro == "SIXEL_REGISTRY_SCALAR_STRING") return 5
    return 0
}
function split_fields(text, fields, position, character, quoted, depth,
                      count, field) {
    quoted = 0
    depth = 0
    count = 1
    field = ""
    for (position = 1; position <= length(text); ++position) {
        character = substr(text, position, 1)
        if (character == "\"" &&
            (position == 1 ||
             substr(text, position - 1, 1) != "\\")) {
            quoted = !quoted
        } else if (!quoted && character == "(") {
            depth += 1
        } else if (!quoted && character == ")" && depth > 0) {
            depth -= 1
        }
        if (!quoted && depth == 0 && character == ",") {
            fields[count] = field
            count += 1
            field = ""
        } else {
            field = field character
        }
    }
    fields[count] = field
    return count
}
function inspect(row, fields, count, option_id, scope, optflag, name,
                 form, default_policy, values, field_index, macro,
                 env_index, env) {
    gsub(/[[:space:]]+/, " ", row)
    match(row, /SIXEL_REGISTRY_[A-Z0-9_]+/)
    macro = substr(row, RSTART, RLENGTH)
    row = substr(row, RSTART + RLENGTH + 1)
    sub(/\),[[:space:]]*$/, "", row)
    count = split_fields(row, fields)
    if (!macro_is_approved(macro)) {
        fail("unapproved top-level option schema macro: " macro)
        return
    }
    if (count != expected_field_count(macro)) {
        fail("malformed top-level option schema: " row)
        return
    }
    for (field_index = 1; field_index <= count; ++field_index) {
        sub(/^[[:space:]]*/, "", fields[field_index])
        sub(/[[:space:]]*$/, "", fields[field_index])
    }
    option_id = fields[1]
    scope = fields[2]
    optflag = fields[3]
    name = fields[4]
    if (option_id !~ /^SIXEL_OPTION_SCHEMA_[A-Z0-9_]+$/) {
        fail("top-level schema has an invalid id: " option_id)
    }
    if (scope != "SIXEL_OPTION_SCOPE_ENCODER" &&
        scope != "SIXEL_OPTION_SCOPE_DECODER" &&
        scope != "SIXEL_OPTION_SCOPE_ALL") {
        fail(option_id " has an invalid scope: " scope)
    }
    if (optflag == "SIXEL_OPTION_NO_OPTFLAG") {
        if (macro !~ /^SIXEL_REGISTRY_SCALAR_/) {
            fail(option_id " environment-only schema must be scalar")
        }
    } else if (optflag !~ /^SIXEL_OPTFLAG_[A-Z0-9_]+$/) {
        fail(option_id " has no registered option flag")
    }
    if (name !~ /^"[a-z0-9][a-z0-9-]*"$/ || name ~ /-"$/) {
        fail(option_id " has a noncanonical long option name: " name)
    }
    if (macro == "SIXEL_REGISTRY_OPTION_SCHEMA" ||
        macro == "SIXEL_REGISTRY_RUNTIME_OPTION_SCHEMA" ||
        macro == "SIXEL_REGISTRY_DIAGNOSTICS_OPTION_SCHEMA" ||
        macro == "SIXEL_REGISTRY_CLIPBOARD_OPTION_SCHEMA") {
        if (macro == "SIXEL_REGISTRY_RUNTIME_OPTION_SCHEMA" ||
            macro == "SIXEL_REGISTRY_DIAGNOSTICS_OPTION_SCHEMA" ||
            macro == "SIXEL_REGISTRY_CLIPBOARD_OPTION_SCHEMA") {
            default_policy = "SIXEL_OPTION_DEFAULT_FIXED"
            values = fields[6]
        } else {
        form = fields[5]
        default_policy = fields[6]
        values = fields[8]
        if (form !~ /^SIXEL_OPTION_ARGUMENT_(SINGLE|LIST)$/) {
            fail(option_id " has an invalid argument form: " form)
        }
        if (default_policy !~ /^SIXEL_OPTION_DEFAULT_(FIXED|OWNER)$/) {
            fail(option_id " has no explicit default policy")
        }
        if (fields[7] == "") {
            fail(option_id " has no explicit default value")
        }
        }
        if (values !~ /^g_[a-z0-9_]+_values$/) {
            fail(option_id " has no registered value table: " values)
        }
    } else {
        env_index = (macro == "SIXEL_REGISTRY_SCALAR_BOOLEAN" ||
                     macro == "SIXEL_REGISTRY_SCALAR_UINT" ||
                     macro == "SIXEL_REGISTRY_SCALAR_STRING") ? 5 : 7
        env = fields[env_index]
        if (env !~ /^"[A-Z][A-Z0-9_]+"$/) {
            fail(option_id " has no registered environment variable")
        }
    }
    if (macro == "SIXEL_REGISTRY_SCALAR_BOOLEAN" &&
        fields[7] !~ /^[01]$/) {
        fail(option_id " has no boolean default")
    }
    if (seen_id[option_id]) {
        fail("duplicate top-level schema id: " option_id)
    }
    seen_id[option_id] = 1
    rows += 1
}
BEGIN {
    in_registry = 0
    in_row = 0
    failed = 0
    rows = 0
    raw_initializer = 0
    unknown_initializer = 0
    unknown_preprocessor = 0
}
/g_options\[\][[:space:]]*=[[:space:]]*\{/ {
    in_registry = 1
    next
}
in_registry && /^[[:space:]]*};/ {
    in_registry = 0
    next
}
in_registry && \
        /SIXEL_REGISTRY_(OPTION_SCHEMA|RUNTIME_OPTION_SCHEMA|DIAGNOSTICS_OPTION_SCHEMA|CLIPBOARD_OPTION_SCHEMA|SCALAR_[A-Z0-9_]+)\(/ {
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
in_registry && !in_row && /^[[:space:]]*#/ &&
        $0 !~ /^[[:space:]]*#[[:space:]]*(if|elif|else|endif)([[:space:]]|$)/ {
    unknown_preprocessor = 1
}
in_registry && !in_row && /[^[:space:]]/ &&
        $0 !~ /^[[:space:]]*#/ {
    unknown_initializer = 1
}
END {
    if (in_row) {
        fail("unterminated top-level option schema")
    }
    if (raw_initializer || unknown_initializer || unknown_preprocessor) {
        fail("top-level option schema bypasses approved registry macros")
    }
    if (rows == 0) {
        fail("top-level option registry has no rows")
    }
    exit failed ? 1 : 0
}
' "$registry_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

test -f "$public_header" -a -f "$help_file" -a -f "$man_file" \
    -a -f "$decoder_help_file" -a -f "$decoder_man_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# option compatibility surface is incomplete"
    exit 0
}

# Every public long option must return the same one-character code accepted by
# the converter short-option parser. This covers unregistered flag options as
# well as structured options from the registry.
awk '
function fail(message) {
    print "# " message
    failed = 1
}
function inspect_long_option(line, fields, count, name, value, short_name,
                             option_position, next_character) {
    gsub(/[[:space:]]+/, "", line)
    sub(/,\/\*.*$/, "", line)
    sub(/,$/, "", line)
    if (line == "{0,0,0,0}") {
        in_long_options = 0
        return
    }
    count = split(line, fields, ",")
    if (count != 4 || fields[1] !~ /^\{"[a-z0-9][a-z0-9-]*"$/ ||
            fields[2] !~ /^(no_argument|required_argument|optional_argument)$/ ||
            fields[3] !~ /^(NULL|&long_opt)$/) {
        fail(FILENAME ": malformed public long option: " line)
        return
    }
    value = fields[4]
    sub(/\}.*/, "", value)
    if (value !~ /^\047.\047$/) {
        fail(FILENAME ": long option has no one-character short form: " line)
        return
    }
    name = fields[1]
    sub(/^\{"/, "", name)
    sub(/"$/, "", name)
    short_name = substr(value, 2, 1)
    option_position = index(option_string[FILENAME], short_name)
    if (option_position == 0) {
        fail(FILENAME ": --" name " is absent from the short option string")
        return
    }
    next_character = substr(option_string[FILENAME], option_position + 1, 1)
    if (fields[2] == "required_argument" && next_character != ":") {
        fail(FILENAME ": -" short_name " lacks its required argument marker")
    }
    if (fields[2] == "optional_argument" &&
            substr(option_string[FILENAME], option_position + 1, 2) != "::") {
        fail(FILENAME ": -" short_name " lacks its optional argument marker")
    }
    if (fields[2] == "no_argument" && next_character == ":") {
        fail(FILENAME ": -" short_name " unexpectedly requires an argument")
    }
    rows[FILENAME] += 1
}
BEGIN {
    failed = 0
    in_option_string = 0
    in_long_options = 0
}
/^static char const g_[a-z0-9_]+_optstring\[\][[:space:]]*=/ {
    in_option_string = 1
}
in_option_string {
    line = $0
    while (match(line, /"[^"]*"/)) {
        option_string[FILENAME] = option_string[FILENAME] \
            substr(line, RSTART + 1, RLENGTH - 2)
        line = substr(line, RSTART + RLENGTH)
    }
    if ($0 ~ /;[[:space:]]*$/) {
        in_option_string = 0
    }
    next
}
/struct option long_options\[\][[:space:]]*=[[:space:]]*\{/ {
    in_long_options = 1
    next
}
in_long_options && /^[[:space:]]*\{/ {
    inspect_long_option($0)
}
END {
    for (file in option_string) {
        if (rows[file] == 0) {
            fail(file ": public long option table has no rows")
        }
    }
    exit failed ? 1 : 0
}
' "$help_file" "$decoder_help_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

# CLI-exposed schemas retain their short/long pair across discovery surfaces.
# An explicit environment-only scalar must have no dedicated getopt entry;
# it is still listed by the independent environment-help consistency check.
awk -v registry_file="$registry_file" \
    -v header_file="$public_header" \
    -v encoder_file="$help_file" \
    -v encoder_man="$man_file" \
    -v decoder_file="$decoder_help_file" \
    -v decoder_man="$decoder_man_file" '
function fail(message) {
    print "# " message
    failed = 1
}
function trim(text) {
    sub(/^[[:space:]]*/, "", text)
    sub(/[[:space:]]*$/, "", text)
    return text
}
function check_surface(option_id, name, short_name, source_file,
                       manual_file, getopt_needle, help_needle) {
    getopt_needle = "{\"" name "\",required_argument"
    help_needle = "{\047" short_name "\047,\"" name "\""
    if (index(source[source_file], getopt_needle) == 0) {
        fail(source_file " getopt table omits --" name)
    }
    if (index(source[source_file], help_needle) == 0) {
        fail(source_file " help omits -" short_name "/--" name)
    }
    if (index(manual[manual_file], "--" name) == 0) {
        fail(manual_file " omits --" name)
    }
    if (index(manual[manual_file], "-" short_name) == 0) {
        fail(manual_file " omits -" short_name)
    }
}
function inspect(row, fields, count, option_id, scope, optflag, name,
                 short_name, converter_scope) {
    gsub(/[[:space:]]+/, " ", row)
    match(row, /SIXEL_REGISTRY_[A-Z0-9_]+/)
    row = substr(row, RSTART + RLENGTH + 1)
    sub(/\),[[:space:]]*$/, "", row)
    count = split(row, fields, /,[[:space:]]*/)
    option_id = trim(fields[1])
    scope = trim(fields[2])
    optflag = trim(fields[3])
    name = trim(fields[4])
    gsub(/^"|"$/, "", name)
    if (optflag == "SIXEL_OPTION_NO_OPTFLAG") {
        if (index(source[encoder_file], "{\"" name "\",") != 0 ||
            index(source[decoder_file], "{\"" name "\",") != 0) {
            fail(option_id " environment-only schema acquired a CLI flag")
        }
        return
    }
    short_name = option_short[optflag]
    if (short_name == "") {
        fail(option_id " has no one-character short option: " optflag)
        return
    }
    converter_scope = 0
    if (scope ~ /SIXEL_OPTION_SCOPE_ENCODER/ ||
        scope ~ /SIXEL_OPTION_SCOPE_ALL/) {
        check_surface(option_id, name, short_name, encoder_file, encoder_man)
        converter_scope = 1
    }
    if (scope ~ /SIXEL_OPTION_SCOPE_DECODER/ ||
        scope ~ /SIXEL_OPTION_SCOPE_ALL/) {
        check_surface(option_id, name, short_name, decoder_file, decoder_man)
        converter_scope = 1
    }
    if (!converter_scope) {
        fail(option_id " has no converter scope")
    }
}
BEGIN {
    failed = 0
    in_registry = 0
    in_row = 0
}
FILENAME == header_file {
    if ($0 ~ /^#define[[:space:]]+SIXEL_OPTFLAG_[A-Z0-9_]+/ &&
        match($0, /\(\047.\047\)/)) {
        split($0, header_fields, /[[:space:]]+/)
        option_short[header_fields[2]] = substr($0, RSTART + 2, 1)
    }
    next
}
FILENAME == registry_file {
    if ($0 ~ /g_options\[\][[:space:]]*=[[:space:]]*\{/) {
        in_registry = 1
        next
    }
    if (in_registry && $0 ~ /^[[:space:]]*};/) {
        in_registry = 0
        next
    }
    if (in_registry &&
        $0 ~ /SIXEL_REGISTRY_(OPTION_SCHEMA|RUNTIME_OPTION_SCHEMA|DIAGNOSTICS_OPTION_SCHEMA|CLIPBOARD_OPTION_SCHEMA|SCALAR_[A-Z0-9_]+)\(/) {
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
FILENAME == encoder_file || FILENAME == decoder_file {
    line = $0
    gsub(/[[:space:]]+/, "", line)
    source[FILENAME] = source[FILENAME] line
    next
}
FILENAME == encoder_man || FILENAME == decoder_man {
    line = $0
    gsub(/\\\(ti/, "~", line)
    gsub(/\\/, "", line)
    gsub(/[[:space:]]+/, "", line)
    manual[FILENAME] = manual[FILENAME] line
}
END {
    exit failed ? 1 : 0
}
' "$public_header" "$help_file" "$man_file" \
    "$decoder_help_file" "$decoder_man_file" "$registry_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

awk '
function fail(message) {
    print "# " message
    failed = 1
}
function macro_is_approved(macro) {
    return macro == "SIXEL_REGISTRY_DEQUANTIZE_CHOICE" ||
        macro == "SIXEL_REGISTRY_DEQUANTIZE_UINT" ||
        macro == "SIXEL_REGISTRY_DECODER_SIZE" ||
        macro == "SIXEL_REGISTRY_ENCODER_BOOLEAN" ||
        macro == "SIXEL_REGISTRY_ENCODER_CHOICE" ||
        macro == \
            "SIXEL_REGISTRY_ENCODER_CHOICE_ENV_PARSE_SIGNED_LONG" ||
        macro == "SIXEL_REGISTRY_ENCODER_CHOICE_ENV" ||
        macro == "SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE" ||
        macro == "SIXEL_REGISTRY_ENCODER_DOUBLE" ||
        macro == "SIXEL_REGISTRY_ENCODER_DOUBLE_ENV_CLAMP" ||
        macro == "SIXEL_REGISTRY_ENCODER_FLOAT" ||
        macro == "SIXEL_REGISTRY_ENCODER_FLOAT_RANGE" ||
        macro == "SIXEL_REGISTRY_ENCODER_INT" ||
        macro == "SIXEL_REGISTRY_ENCODER_INT_PAIR" ||
        macro == "SIXEL_REGISTRY_ENCODER_RATIO3" ||
        macro == "SIXEL_REGISTRY_ENCODER_MIRROR_CHOICE" ||
        macro == "SIXEL_REGISTRY_ENCODER_MULTI_CHOICE" ||
        macro == "SIXEL_REGISTRY_ENCODER_SCALED_U8_ENV_CLAMP" ||
        macro == "SIXEL_REGISTRY_ENCODER_SIZE" ||
        macro == "SIXEL_REGISTRY_ENCODER_POSITIVE_SIZE" ||
        macro == "SIXEL_REGISTRY_ENCODER_UINT" ||
        macro == "SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE" ||
        macro == \
            "SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_MAXIMUM_SIGNED" ||
        macro == "SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_SIGNED" ||
        macro == "SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED" ||
        macro == "SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG" ||
        macro == "SIXEL_REGISTRY_LOADER_BOOLEAN" ||
        macro == "SIXEL_REGISTRY_LOADER_CHOICE" ||
        macro == "SIXEL_REGISTRY_LOADER_CHOICE_LIST" ||
        macro == "SIXEL_REGISTRY_LOADER_CHOICE_ENV" ||
        macro == "SIXEL_REGISTRY_LOADER_DOUBLE" ||
        macro == "SIXEL_REGISTRY_LOADER_UINT" ||
        macro == "SIXEL_REGISTRY_LOADER_UINT_ENV_CLAMP_MAXIMUM" ||
        macro == "SIXEL_REGISTRY_LOADER_UINT_ENV_REJECT_SIGNED" ||
        macro == \
            "SIXEL_REGISTRY_LOADER_UINT_ENV_CLAMP_MAXIMUM_DIGITS" ||
        macro == "SIXEL_REGISTRY_LOADER_SIZE_ENV_ERROR" ||
        macro == "SIXEL_REGISTRY_RUNTIME_SIZE" ||
        macro == "SIXEL_REGISTRY_RUNTIME_UINT" ||
        macro == "SIXEL_REGISTRY_RUNTIME_INT" ||
        macro == "SIXEL_REGISTRY_RUNTIME_CHOICE" ||
        macro == "SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN" ||
        macro == "SIXEL_REGISTRY_DIAGNOSTICS_INT" ||
        macro == "SIXEL_REGISTRY_DIAGNOSTICS_STRING" ||
        macro == "SIXEL_REGISTRY_CLIPBOARD_STRING" ||
        macro == "SIXEL_REGISTRY_COLOR_STRING" ||
        macro == "SIXEL_REGISTRY_OUTPUT_COLOR_CHOICE" ||
        macro == "SIXEL_REGISTRY_COMPLETION_STRING"
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
    if (macro !~ /DEQUANTIZE|DECODER|ENCODER|LOADER|RUNTIME|DIAGNOSTICS|CLIPBOARD|COMPLETION|COLOR_STRING|OUTPUT_COLOR_CHOICE/) {
        fail(option_id ":" name " needs a typed target binding")
    }
    if (!macro_is_approved(macro)) {
        fail(option_id ":" name " uses unapproved row macro " macro)
    }
    if (macro ~ /MULTI/ &&
            base !~ /^SIXEL_[A-Z0-9_]+_BASE_SET_[A-Z0-9_]+$/) {
        fail(option_id ":" name " has no explicit multi-base set")
    }
    if (macro !~ /MULTI/ &&
            base ~ /^SIXEL_[A-Z0-9_]+_BASE_SET_[A-Z0-9_]+$/) {
        fail(option_id ":" name " uses a base set with a single-base macro")
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
    unknown_initializer = 0
    unknown_preprocessor = 0
}
/(g_suboptions|g_img2sixel_completion_policy_keys)\[\][[:space:]]*=[[:space:]]*\{/ {
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
in_registry && !in_row && /^[[:space:]]*#/ &&
        $0 !~ /^[[:space:]]*#[[:space:]]*(if|elif|else|endif)([[:space:]]|$)/ {
    unknown_preprocessor = 1
}
in_registry && !in_row && /[^[:space:]]/ &&
        $0 !~ /^[[:space:]]*#/ {
    unknown_initializer = 1
}
END {
    if (in_row) {
        fail("unterminated suboption registry row")
    }
    if (raw_initializer) {
        fail("raw suboption initializer bypasses registry macros")
    }
    if (unknown_initializer) {
        fail("unknown suboption initializer bypasses registry macros")
    }
    if (unknown_preprocessor) {
        fail("unsupported preprocessor directive in suboption registry")
    }
    if (rows == 0) {
        fail("suboption registry has no rows")
    }
    exit failed ? 1 : 0
}
' "$registry_file" "$completion_registry_file" || {
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
    -v completion_registry_file="$completion_registry_file" \
    -v help_file="$help_file" \
    -v man_file="$man_file" \
    -v decoder_help_file="$decoder_help_file" \
    -v decoder_man_file="$decoder_man_file" '
function fail(message) {
    print "# " message
    failed = 1
}
function loader_scope(base, token) {
    if (base == "NULL") {
        return "common"
    }
    if (!match(base, /SIXEL_LOADER_INDEX_[A-Z0-9_]+/)) {
        fail("loader suboption has an unresolved scope: " base)
        return ""
    }
    token = substr(base, RSTART, RLENGTH)
    sub(/^SIXEL_LOADER_INDEX_/, "", token)
    return tolower(token)
}
function inspect(row, fields, count, name, alias, key, macro, scope,
                 base, environment, environment_fallback,
                 environment_legacy, environment_mapping,
                 environment_scope) {
    gsub(/[[:space:]]+/, " ", row)
    match(row, /SIXEL_REGISTRY_[A-Z0-9_]+/)
    macro = substr(row, RSTART, RLENGTH)
    sub(/^.*SIXEL_REGISTRY_[A-Z0-9_]+\(/, "", row)
    sub(/\),[[:space:]]*$/, "", row)
    count = split(row, fields, /,[[:space:]]*/)
    name = fields[3]
    alias = fields[4]
    base = fields[2]
    environment = fields[5]
    environment_fallback = fields[6]
    environment_legacy = fields[7]
    gsub(/^"|"$/, "", name)
    gsub(/^\047|\047$/, "", alias)
    gsub(/[[:space:]]/, "", base)
    gsub(/[[:space:]"]/, "", environment)
    gsub(/[[:space:]"]/, "", environment_fallback)
    gsub(/[[:space:]"]/, "", environment_legacy)
    key = name SUBSEP alias
    expected[key] = 1
    expected_name[key] = name
    expected_alias[key] = alias
    scope = ""
    if (macro == "SIXEL_REGISTRY_COLOR_STRING") scope = fields[6]
    if (macro == "SIXEL_REGISTRY_RUNTIME_SIZE") scope = fields[8]
    if (macro == "SIXEL_REGISTRY_RUNTIME_UINT" ||
        macro == "SIXEL_REGISTRY_RUNTIME_INT") scope = fields[10]
    if (macro == "SIXEL_REGISTRY_RUNTIME_CHOICE") scope = fields[8]
    if (macro == "SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN" ||
        macro == "SIXEL_REGISTRY_DIAGNOSTICS_INT" ||
        macro == "SIXEL_REGISTRY_DIAGNOSTICS_STRING") scope = fields[6]
    if (macro == "SIXEL_REGISTRY_CLIPBOARD_STRING") {
        scope = "SIXEL_OPTION_SCOPE_ALL"
    }
    if (macro == "SIXEL_REGISTRY_COMPLETION_STRING") {
        scope = "SIXEL_OPTION_SCOPE_ENCODER"
    }
    if (scope ~ /SIXEL_OPTION_SCOPE_ALL/) {
        expected_help[key] = help_file
        expected_man[key] = man_file
        expected_second_help[key] = decoder_help_file
        expected_second_man[key] = decoder_man_file
    } else if (macro ~ /DEQUANTIZE|DECODER/ ||
        scope ~ /SIXEL_REGISTRY_DECODER_CONSUMER_SCOPE/) {
        expected_help[key] = decoder_help_file
        expected_man[key] = decoder_man_file
    } else {
        expected_help[key] = help_file
        expected_man[key] = man_file
    }
    if (macro ~ /LOADER_/) {
        environment_scope = loader_scope(base)
        if (environment_scope != "") {
            environment_mapping = environment_scope ":" name "=" \
                environment
            expected_loader_environment[environment_mapping] = 1
            expected_loader_environment_name[environment] = 1
            if (environment_fallback != "NULL") {
                expected_loader_environment_name[environment_fallback] = 1
            }
            if (environment_legacy != "NULL") {
                expected_loader_environment_name[environment_legacy] = 1
            }
        }
    }
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
        if (!mapping_is_documented(document[expected_help[key]],
                                   name, alias)) {
            fail(expected_help[key] " omits " name "=" alias)
        }
        if (!mapping_is_documented(document[expected_man[key]],
                                   name, alias)) {
            fail(expected_man[key] " omits " name "=" alias)
        }
        if (expected_second_help[key] != "" &&
                !mapping_is_documented(document[expected_second_help[key]],
                                       name, alias)) {
            fail(expected_second_help[key] " omits " name "=" alias)
        }
        if (expected_second_man[key] != "" &&
                !mapping_is_documented(document[expected_second_man[key]],
                                       name, alias)) {
            fail(expected_second_man[key] " omits " name "=" alias)
        }
    }
    for (environment_mapping in expected_loader_environment) {
        if (index(document[help_file], environment_mapping) == 0) {
            fail(help_file " omits loader environment mapping " \
                 environment_mapping)
        }
        if (index(document[man_file], environment_mapping) == 0) {
            fail(man_file " omits loader environment mapping " \
                 environment_mapping)
        }
    }
    for (environment in expected_loader_environment_name) {
        if (index(document[help_file], environment) == 0) {
            fail(help_file " omits loader environment " environment)
        }
        if (index(document[man_file], environment) == 0) {
            fail(man_file " omits loader environment " environment)
        }
    }
    exit failed ? 1 : 0
}
' "$registry_file" "$completion_registry_file" "$help_file" "$man_file" \
    "$decoder_help_file" "$decoder_man_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

# Consumer selectors contain field tokens rather than copied environment
# names.  Derive the accepted identifiers from the registry rows themselves
# so this check does not become a second hand-maintained option table.
awk -v registry_file="$registry_file" '
function fail(message) {
    print "# " message
    failed = 1
}
function binding_from_registry(row, fields, count, macro, binding) {
    gsub(/[[:space:]]+/, " ", row)
    match(row, /SIXEL_REGISTRY_[A-Z0-9_]+/)
    macro = substr(row, RSTART, RLENGTH)
    sub(/^.*SIXEL_REGISTRY_[A-Z0-9_]+\(/, "", row)
    sub(/^[[:space:]]*/, "", row)
    sub(/\),[[:space:]]*$/, "", row)
    count = split(row, fields, /,[[:space:]]*/)
    if (macro ~ /ENCODER_SIZE|DECODER_SIZE|LOADER_SIZE_ENV_ERROR|RUNTIME_|DIAGNOSTICS_|CLIPBOARD_|COMPLETION_|COLOR_STRING/) {
        binding = fields[count - 1] "," fields[count]
    } else if (macro ~ /ENCODER_MIRROR_CHOICE|ENCODER_INT_PAIR/) {
        binding = fields[count - 2] "," fields[count - 1] "," \
            fields[count]
    } else if (macro ~ /ENCODER_DIRECT_CHOICE|OUTPUT_COLOR_CHOICE|DEQUANTIZE_|LOADER_/) {
        binding = fields[count]
    } else if (macro ~ /ENCODER_/) {
        binding = fields[count - 1] "," fields[count]
    } else {
        fail("registry row has no selector binding: " row)
        return ""
    }
    gsub(/[[:space:]]+/, "", binding)
    return binding
}
function inspect_selector(text, binding) {
    sub(/^.*SIXEL_SUBOPTION_BINDING_ID_[123]\(/, "", text)
    sub(/\).*$/, "", text)
    binding = text
    gsub(/[[:space:]]+/, "", binding)
    if (binding == "" || !registry_binding[binding]) {
        fail(selector_file ":" selector_line \
             " selects no registry binding: " binding)
    }
}
BEGIN {
    in_registry = 0
    in_row = 0
    in_selector = 0
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
        binding = binding_from_registry(row)
        if (binding != "") {
            registry_binding[binding] = 1
        }
        in_row = 0
        row = ""
    }
    next
}
in_selector {
    selector = selector " " $0
    if ($0 ~ /\)/) {
        inspect_selector(selector)
        in_selector = 0
        selector = ""
    }
    next
}
/SIXEL_SUBOPTION_BINDING_ID_[123]\(/ {
    in_selector = 1
    selector = $0
    selector_file = FILENAME
    selector_line = FNR
    if ($0 ~ /\)/) {
        inspect_selector(selector)
        in_selector = 0
        selector = ""
    }
}
END {
    if (in_selector) {
        fail(selector_file ":" selector_line \
             " has an unterminated registry binding selector")
    }
    exit failed ? 1 : 0
}
' "$registry_file" "$src_root"/src/*.c || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

regression_dir=$src_root/tests/cli/options/regression
test -d "$regression_dir" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# missing per-suboption image regression tests"
    exit 0
}

awk -v registry_file="$registry_file" \
    -v completion_registry_file="$completion_registry_file" \
    -f "$src_root/tests/_static/awk/suboption-image-coverage.awk" \
    "$registry_file" "$completion_registry_file" "$regression_dir"/*.t || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

# Terminal auto intent is a CLI-only extension of -U. Audit its typed
# converter-owned row and discovery/image observations without moving it
# into the library registry or exposing auto through the library setopt API.
awk -v color_file="$src_root/converters/terminal-color-job.c" \
    -v help_file="$help_file" -v man_file="$man_file" \
    -v bash_file="$src_root/converters/shell-completion/bash/img2sixel" \
    -v zsh_file="$src_root/converters/shell-completion/zsh/_img2sixel" '
function fail(message) { print "# " message; failed = 1 }
function inspect(text, fields, count, i) {
    sub(/^.*IMG2SIXEL_REGISTRY_COLOR_INT\(/, "", text)
    sub(/\),[[:space:]]*$/, "", text)
    gsub(/[[:space:]]|"|\047/, "", text)
    count = split(text, fields, ",")
    if (count != 9 || fields[1] != "auto" ||
        fields[2] !~ /^[a-z][a-z0-9_]*$/ || fields[3] !~ /^[A-Z]$/ ||
        fields[4] !~ /^SIXEL_[A-Z0-9_]+$/ ||
        fields[5] !~ /^[a-z][a-z0-9_]*$/ ||
        fields[6] !~ /^[a-z][a-z0-9_]*$/ ||
        fields[7] !~ /^[0-9]+$/ || fields[8] !~ /^[0-9]+$/ ||
        fields[9] !~ /^[0-9]+$/ || fields[7] + 0 > fields[9] + 0 ||
        fields[9] + 0 > fields[8] + 0) {
        fail("invalid converter color registry row")
    }
    key = fields[2]; short = fields[3]; env = fields[4]; initial = fields[9]
    ++rows
}
FILENAME == color_file {
    if (/g_img2sixel_color_policy_keys\[\][[:space:]]*=[[:space:]]*\{/) {
        table = 1; next
    }
    if (table && /^[[:space:]]*};/) { table = 0; next }
    if (table && /IMG2SIXEL_REGISTRY_COLOR_INT\(/) {
        row = $0; in_row = 1; next
    }
    if (table && in_row) {
        row = row " " $0
        if (/\),[[:space:]]*$/) { inspect(row); in_row = 0 }
        next
    }
    if (table && /[^[:space:]]/) fail("unregistered converter color row")
    next
}
/^# CLI-color: auto:/ { observed[$4] = 1 }
{
    text = $0
    gsub(/[^[:alnum:]_=]/, "", text)
    surface[FILENAME] = surface[FILENAME] text
}
END {
    if (rows != 1 || in_row) fail("missing or incomplete color registry")
    if (index(surface[help_file], key "=" short) == 0 ||
        index(surface[man_file], key "=" short) == 0 ||
        index(surface[help_file], env) == 0 ||
        index(surface[man_file], env) == 0) {
        fail("color suboption help/manual mapping is incomplete")
    }
    if (index(surface[bash_file], key "=") == 0 ||
        index(surface[zsh_file], key "=") == 0 ||
        index(surface[bash_file], short initial) == 0 ||
        index(surface[zsh_file], short initial) == 0) {
        fail("color suboption completion is incomplete")
    }
    if (!observed["long"] || !observed["compact"] ||
        !observed["environment"] || !observed["precedence"] ||
        !observed["invalid"]) {
        fail("color suboption lacks real CLI image observations")
    }
    exit failed ? 1 : 0
}
' "$src_root/converters/terminal-color-job.c" "$help_file" "$man_file" \
    "$src_root/converters/shell-completion/bash/img2sixel" \
    "$src_root/converters/shell-completion/zsh/_img2sixel" \
    "$src_root/tests/cli/color/session/"*.t || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

echo "ok 1 - suboptions use one complete registry"
exit 0
