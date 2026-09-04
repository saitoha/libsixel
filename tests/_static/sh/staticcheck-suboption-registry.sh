#!/bin/sh
# Emit TAP for the single suboption registry and its mandatory metadata.

set -eux

echo "1..1"
set -v

src_root=$1
registry_file=$src_root/src/options-registry.c
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
    if (optflag !~ /^SIXEL_OPTFLAG_[A-Z0-9_]+$/) {
        fail(option_id " has no registered short option")
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
        env_index = (macro == "SIXEL_REGISTRY_SCALAR_UINT" ||
                     macro == "SIXEL_REGISTRY_SCALAR_STRING") ? 5 : 7
        env = fields[env_index]
        if (env !~ /^"[A-Z][A-Z0-9_]+"$/) {
            fail(option_id " has no registered environment variable")
        }
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

# Until getopt tables are generated, require every structured registry option
# to retain the same short/long pair in converter parsing, help, and manuals.
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
    if (index(manual[manual_file], "-" short_name) == 0 ||
        index(manual[manual_file], "--" name) == 0) {
        fail(manual_file " omits -" short_name "/--" name)
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
    short_name = option_short[optflag]
    if (short_name == "") {
        fail(option_id " has an unresolved short option " optflag)
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
        macro == "SIXEL_REGISTRY_ENCODER_INT" ||
        macro == "SIXEL_REGISTRY_ENCODER_INT_PAIR" ||
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
    if (macro !~ /DEQUANTIZE|DECODER|ENCODER|LOADER|RUNTIME|DIAGNOSTICS|CLIPBOARD|COMPLETION/) {
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
    -v man_file="$man_file" \
    -v decoder_help_file="$decoder_help_file" \
    -v decoder_man_file="$decoder_man_file" '
function fail(message) {
    print "# " message
    failed = 1
}
function inspect(row, fields, count, name, alias, key, macro, scope) {
    gsub(/[[:space:]]+/, " ", row)
    match(row, /SIXEL_REGISTRY_[A-Z0-9_]+/)
    macro = substr(row, RSTART, RLENGTH)
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
    scope = ""
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
    exit failed ? 1 : 0
}
' "$registry_file" "$help_file" "$man_file" \
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
    if (macro ~ /ENCODER_SIZE|DECODER_SIZE|LOADER_SIZE_ENV_ERROR|RUNTIME_|DIAGNOSTICS_|CLIPBOARD_|COMPLETION_/) {
        binding = fields[count - 1] "," fields[count]
    } else if (macro ~ /ENCODER_MIRROR_CHOICE|ENCODER_INT_PAIR/) {
        binding = fields[count - 2] "," fields[count - 1] "," \
            fields[count]
    } else if (macro ~ /ENCODER_DIRECT_CHOICE|DEQUANTIZE_|LOADER_/) {
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

awk -v registry_file="$registry_file" '
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
               name == "pin_threads")) ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_SIERRA/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_A_DITHER/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_X_DITHER/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_INTERFRAME/ ||
             fields[2] ~ /SIXEL_DIFFUSION_BASE_STBN/)) {
        expected_dither_contract[key] = 1
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_LUT_POLICY") {
        expected_lookup_contract[key] = 1
    }
    if (option_id == "SIXEL_OPTION_SCHEMA_MERGE_POLICY" ||
            option_id == "SIXEL_OPTION_SCHEMA_COVER_POLICY" ||
            (option_id == "SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL" &&
             name == "sample_target")) {
        expected_palette_contract[key] = 1
    }
    if (macro ~ /MULTI/) {
        expected_multi_base_contract[key] = 1
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
    if (option_id == "SIXEL_OPTION_SCHEMA_COMPLETION_POLICY") {
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
    } else if (macro ~ /ENCODER_DIRECT_CHOICE/) {
        binding = fields[count]
        binding_value = fields[count]
    } else if (macro ~ /ENCODER_/) {
        binding = fields[count - 1] "|" fields[count]
        binding_value = fields[count - 1]
        binding_override = fields[count]
    } else if (macro ~ /DEQUANTIZE_|LOADER_/) {
        binding = fields[count]
    } else if (macro ~ /RUNTIME_|DIAGNOSTICS_|CLIPBOARD_|COMPLETION_/) {
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
FILENAME != registry_file {
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
            "|stored=1|binding=" expected_trace_binding[key]) > 0) {
        has_short_contract[FILENAME] = 1
    }
    if (index($0, "env_trace#*LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key]) > 0) {
        has_environment_contract[FILENAME] = 1
    }
    if (index($0, "short_trace#*LSXCMP1|*key=" expected_name[key] \
            "|configured=1|override=1|used=1*") > 0) {
        has_short_completion_contract[FILENAME] = 1
    }
    if (index($0, "env_trace#*LSXCMP1|*key=" expected_name[key] \
            "|configured=1|override=0|used=1*") > 0) {
        has_environment_completion_contract[FILENAME] = 1
    }
    if (index($0, "short_trace#*LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key] \
            "|value=") > 0) {
        has_short_value_contract[FILENAME] = 1
    }
    if (index($0, "env_trace#*LSXSUB1|*key=" expected_name[key] \
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
    if (index($0, "range_trace#*LSXSUB1|*key=" expected_name[key] \
            "|stored=1|binding=" expected_trace_binding[key] \
            "|value=") > 0) {
        has_range_value_contract[FILENAME] = 1
    }
    if (index($0, "range_trace#*LSXSUB1|*key=" expected_name[key] \
            "|stored=1") > 0 && index($0, \
            " = \"${range_trace}\"") > 0) {
        has_range_reject_contract[FILENAME] = 1
    }
    if (index($0, "width_trace#*LSXSUB1|*key=" expected_name[key] \
            "|stored=1") > 0 && index($0, \
            " = \"${width_trace}\"") > 0) {
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
        is_completion = expected_option[key] == \
            "SIXEL_OPTION_SCHEMA_COMPLETION_POLICY"
        if (test_binding[file] != expected_binding[key]) {
            fail(file " binding does not match registry field selection")
        }
        if (!is_completion && !has_lsqa[file]) {
            fail(file " does not validate decoded image quality")
        }
        if (!is_completion && !has_compare[file]) {
            fail(file " does not compare short and environment output")
        }
        if (!is_completion && (!has_local_artifact_dir[file] ||
                !has_unique_short_output[file] ||
                !has_unique_environment_output[file])) {
            fail(file " does not isolate output in its local artifact dir")
        }
        if (!is_completion && (!has_short_redirection[file] ||
                !has_environment_redirection[file] ||
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
                lookup_contract_count[file] < 2) {
            fail(file " does not verify every multi-base lookup consumer")
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
' "$registry_file" "$regression_dir"/*.t || {
    echo "not ok 1 - suboptions use one complete registry"
    exit 0
}

echo "ok 1 - suboptions use one complete registry"
exit 0
