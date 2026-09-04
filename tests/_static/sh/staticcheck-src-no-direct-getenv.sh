#!/bin/sh
# Emit TAP for banning raw and registered direct environment reads in src.

set -eux

src_root=$1
src_dir=$src_root/src
converters_dir=$src_root/converters
matches=
registered_matches=
resolver_bypass_matches=
getenv_argument_matches=
internal_name_matches=
registry_file=$src_dir/options-registry.c
completion_registry_file=$converters_dir/completion_utils.c

echo "1..1"
set -v

if test ! -d "$src_dir"; then
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# src directory not found: $src_dir"
    exit 0
fi

# shellcheck disable=SC2016
matches=$(find "$src_dir" -maxdepth 1 -type f -name '*.[ch]' \
    ! -name 'compat_stub.*' -print0 | \
    xargs -0 awk '
BEGIN {
    in_block = 0
    found = 0
}
{
    line = $0
    code = ""
    i = 1
    while (i <= length(line)) {
        two = substr(line, i, 2)
        if (in_block) {
            if (two == "*/") {
                in_block = 0
                i += 2
                continue
            }
            i += 1
            continue
        }
        if (two == "/*") {
            in_block = 1
            i += 2
            continue
        }
        if (two == "//") {
            break
        }
        code = code substr(line, i, 1)
        i += 1
    }
    if (code ~ /(^|[^[:alnum:]_])getenv[[:space:]]*\(/) {
        print FILENAME ":" FNR ":" line
        found = 1
    }
}
END {
    if (found) {
        exit 1
    }
    exit 0
}
' || :)

if test -n "$matches"; then
    echo "not ok 1 - src files avoid direct getenv() calls"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 0
fi

# Registry-owned environment names must not be copied into consumers.
# Suboptions select stable field bindings and scalar options select schema
# identifiers, leaving the registry as the only source of environment
# spelling.
registered_matches=$(awk -v registry_file="$registry_file" '
FILENAME == registry_file {
    line = $0
    while (gsub(/"[[:space:]]*"/, "", line) > 0) {
        # Join adjacent C string literals before extracting names.
    }
    while (match(line, /"[A-Z][A-Z0-9_]*_[A-Z0-9_]+"/)) {
        name = substr(line, RSTART + 1, RLENGTH - 2)
        registered[name] = 1
        line = substr(line, RSTART + RLENGTH)
    }
    next
}
{
    line = $0
    while (gsub(/"[[:space:]]*"/, "", line) > 0) {
        # A split literal must not hide a registry-owned name.
    }
    while (match(line, /"[A-Z][A-Z0-9_]*_[A-Z0-9_]+"/)) {
        name = substr(line, RSTART + 1, RLENGTH - 2)
        if (registered[name]) {
            print FILENAME ":" FNR ":" $0
            next
        }
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$registry_file" "$src_dir"/*.[ch])

test -z "$registered_matches" || {
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# registered environment names exist outside the registry"
    printf '%s\n' "$registered_matches" | sed 's/^/# /'
    exit 0
}

# Private test environment names belong only to the typed broker in options.c.
internal_name_matches=$(find "$src_dir" "$converters_dir" -maxdepth 1 \
    -type f \( -name '*.c' -o -name '*.h' \) \
    ! -path "$src_dir/options.c" -exec awk '
/_SIXEL_TEST_[A-Z0-9_]+/ {
    print FILENAME ":" FNR ":" $0
}
' {} +)

test -z "$internal_name_matches" || {
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# private test environment names exist outside the typed broker"
    printf '%s\n' "$internal_name_matches" | sed 's/^/# /'
    exit 0
}

# Raw environment reads must use one literal or a reviewed generic helper.
# Reject every other direct-call shape so line wrapping, comments, and local
# prefix macros cannot hide a registry-owned name.
getenv_argument_matches=$(awk \
    -v registry_file="$registry_file" \
    -v completion_registry_file="$completion_registry_file" '
FILENAME == registry_file {
    line = $0
    while (match(line, /"[A-Z][A-Z0-9_]*_[A-Z0-9_]+"/)) {
        name = substr(line, RSTART + 1, RLENGTH - 2)
        registered[name] = 1
        line = substr(line, RSTART + RLENGTH)
    }
    next
}
FILENAME == completion_registry_file {
    if ($0 ~ /g_img2sixel_completion_policy_keys\[\]/) {
        in_completion_registry = 1
    }
    if (in_completion_registry) {
        line = $0
        while (match(line, /"[A-Z][A-Z0-9_]*_[A-Z0-9_]+"/)) {
            name = substr(line, RSTART + 1, RLENGTH - 2)
            registered[name] = 1
            line = substr(line, RSTART + RLENGTH)
        }
        if ($0 ~ /^[[:space:]]*};/) {
            in_completion_registry = 0
        }
        next
    }
}
function argument_is_reviewed(file, argument) {
    if (file ~ /\/options\.c$/ &&
        (argument == "variable" || argument == "name" ||
         argument == "definition->name" ||
         argument == "key_def->env_name" ||
         argument == "key_def->env_fallback_name" ||
         argument == "key_def->env_legacy_name")) {
        return 1
    }
    if (file ~ /\/options-registry\.c$/ &&
        (argument == "schema->env_name" ||
         argument == "schema->env_fallback_name" ||
         argument == "schema->env_legacy_name" ||
         argument == "key->env_name")) {
        return 1
    }
    if (file ~ /\/lookup-fhedt-(8bit|float32)\.c$/ &&
        (argument == "primary" || argument == "legacy")) {
        return 1
    }
    if (file ~ /\/encoder\.c$/ &&
        (argument == "envvar" ||
         argument == "SIXEL_ENCODER_SIGINT_SYNC_EVENT_ENVVAR" ||
         argument == "SIXEL_ENCODER_SIGINT_SYNC_PID_ENVVAR" ||
         argument == "SIXEL_ENCODER_SIGINT_SYNC_FD_ENVVAR" ||
         argument == "SIXEL_ENCODER_GPU_POLICY_ENVVAR" ||
         argument == "SIXEL_ENCODER_ANIMATION_HIDE_CURSOR_ENVVAR" ||
         argument == "SIXEL_ENCODER_LUT_POLICY_ENVVAR" ||
         argument == "SIXEL_ENCODER_6DELTA_THRESHOLD_ENVVAR" ||
         argument == "SIXEL_ENCODER_6DELTA_ERROR_ENVVAR" ||
         argument == "SIXEL_ENCODER_SAMPLE_TARGET_ENVVAR")) {
        return 1
    }
    if (file ~ /\/decoder\.c$/ &&
        argument == "SIXEL_DECODER_GPU_POLICY_ENVVAR") {
        return 1
    }
    if (file ~ /\/gpu-dequant\.c$/ &&
        argument == "SIXEL_GPU_DEQUANT_THRESHOLD_ENVVAR") {
        return 1
    }
    if (file ~ /\/gpu-palette\.c$/ &&
        argument == "SIXEL_GPU_PALETTE_THRESHOLD_ENVVAR") {
        return 1
    }
    if (file ~ /\/img2sixel\.c$/ && argument == "constchar*name") {
        return 1
    }
    if (file ~ /\/completion_utils\.c$/ &&
        argument == "key->environment") {
        return 1
    }
    return 0
}
function inspect_call(file, line_number, text, compact, argument, name) {
    compact = text
    gsub(/[[:space:]]/, "", compact)
    sub(/^(sixel_compat_getenv|img2sixel_compat_getenv)\(/, "", compact)
    sub(/\).*$/, "", compact)
    argument = compact
    if (argument ~ /^"[A-Za-z0-9_]+"$/) {
        name = substr(argument, 2, length(argument) - 2)
        if (registered[name]) {
            print file ":" line_number ":" text
        }
        return
    }
    if (!argument_is_reviewed(file, argument)) {
        print file ":" line_number ":" text
    }
}
FNR == 1 {
    in_block = 0
    in_call = 0
    call = ""
    splice = ""
}
FILENAME ~ /\/(compat_stub|compat)\.[ch]$/ {
    next
}
{
    line = splice $0
    splice = ""
    if (line ~ /\\$/) {
        sub(/\\$/, "", line)
        splice = line
        next
    }
    code = ""
    i = 1
    while (i <= length(line)) {
        two = substr(line, i, 2)
        if (in_block) {
            if (two == "*/") {
                in_block = 0
                i += 2
                continue
            }
            i += 1
            continue
        }
        if (two == "/*") {
            in_block = 1
            i += 2
            continue
        }
        if (two == "//") {
            break
        }
        code = code substr(line, i, 1)
        i += 1
    }
    rest = code
    while (rest != "") {
        if (in_call) {
            call = call " " rest
        } else {
            if (!match(rest,
                       /(sixel_compat_getenv|img2sixel_compat_getenv)/)) {
                break
            }
            in_call = 1
            call = substr(rest, RSTART)
            call_line = FNR
        }
        if (!match(call,
                   /(sixel_compat_getenv|img2sixel_compat_getenv)[[:space:]]*\(/)) {
            break
        }
        if (!match(call, /\)/)) {
            break
        }
        complete = substr(call, 1, RSTART)
        rest = substr(call, RSTART + 1)
        inspect_call(FILENAME, call_line, complete)
        in_call = 0
        call = ""
    }
}
' "$registry_file" "$completion_registry_file" \
    "$src_dir"/*.[ch] "$converters_dir"/*.[ch])

test -z "$getenv_argument_matches" || {
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# raw environment reads use unreviewed argument expressions"
    printf '%s\n' "$getenv_argument_matches" | sed 's/^/# /'
    exit 0
}

# The typed resolver is the only lower-level access path to registry values.
# Keep raw registry lookup and union-valued resolution inside options.c.
resolver_bypass_matches=$(awk '
FILENAME ~ /\/(options|options-registry)\.[ch]$/ {
    next
}
/sixel_option_registry_suboption_by_environment[[:space:]]*\(/ ||
/sixel_option_resolve_suboption_environment[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
}
' "$src_dir"/*.[ch])

test -z "$resolver_bypass_matches" || {
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# registered suboption environment reads bypass typed resolvers"
    printf '%s\n' "$resolver_bypass_matches" | sed 's/^/# /'
    exit 0
}

echo "ok 1 - src files avoid direct getenv() calls"
exit 0
