#!/bin/sh
# Emit TAP for banning raw and registered direct environment reads in src.

set -eu

src_root=$1
src_dir=$src_root/src
matches=
registered_matches=
resolver_bypass_matches=
registry_file=$src_dir/options-registry.c

echo "1..1"

if test ! -d "$src_dir"; then
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# src directory not found: $src_dir"
    exit 1
fi

# shellcheck disable=SC2016
matches=$(find "$src_dir" -maxdepth 1 -type f -name '*.c' \
    ! -name 'compat_stub.c' -print0 | \
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
    exit 1
fi

# Registered suboptions must resolve environment values through options.c.
# Inspect complete calls so both literal names and simple ENVVAR macros are
# caught even when the call spans two source lines.
registered_matches=$(awk -v registry_file="$registry_file" '
FILENAME == registry_file {
    line = $0
    while (match(line, /"SIXEL_[A-Z0-9_]+"/)) {
        name = substr(line, RSTART + 1, RLENGTH - 2)
        registered[name] = 1
        line = substr(line, RSTART + RLENGTH)
    }
    next
}
{
    line = $0
    if (line ~ /^[[:space:]]*#[[:space:]]*define[[:space:]]+/ &&
        match(line, /"SIXEL_[A-Z0-9_]+"/)) {
        value = substr(line, RSTART + 1, RLENGTH - 2)
        sub(/^[[:space:]]*#[[:space:]]*define[[:space:]]+/, "", line)
        macro = line
        sub(/[[:space:]].*$/, "", macro)
        macro_value[macro] = value
    }
    if (FILENAME ~ /\/(options|options-registry|compat_stub)\.c$/) {
        next
    }
    if (!in_call && line ~ /sixel_compat_getenv[[:space:]]*\(/) {
        in_call = 1
        call = line
        call_file = FILENAME
        call_line = FNR
    } else if (in_call) {
        call = call " " line
    }
    if (in_call && call ~ /\)/) {
        calls[++call_count] = call
        call_files[call_count] = call_file
        call_lines[call_count] = call_line
        in_call = 0
        call = ""
    }
}
END {
    for (call_index = 1; call_index <= call_count; ++call_index) {
        found = 0
        for (name in registered) {
            if (index(calls[call_index], "\"" name "\"") > 0) {
                found = 1
            }
        }
        for (macro in macro_value) {
            macro_pattern = "(^|[^[:alnum:]_])" macro \
                "([^[:alnum:]_]|$)"
            if (registered[macro_value[macro]] &&
                calls[call_index] ~ macro_pattern) {
                found = 1
            }
        }
        if (found) {
            print call_files[call_index] ":" call_lines[call_index] ":" \
                calls[call_index]
        }
    }
}
' "$registry_file" "$src_dir"/*.[ch])

test -z "$registered_matches" || {
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# registered suboption environment reads bypass options.c"
    printf '%s\n' "$registered_matches" | sed 's/^/# /'
    exit 1
}

# The typed resolver is the only lower-level access path to registry values.
# Keep raw registry lookup and union-valued resolution inside options.c.
resolver_bypass_matches=$(awk '
FILENAME ~ /\/(options|options-registry)\.c$/ {
    next
}
/sixel_option_registry_suboption_by_environment[[:space:]]*\(/ ||
/sixel_option_resolve_suboption_environment[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
}
' "$src_dir"/*.c)

test -z "$resolver_bypass_matches" || {
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# registered suboption environment reads bypass typed resolvers"
    printf '%s\n' "$resolver_bypass_matches" | sed 's/^/# /'
    exit 1
}

echo "ok 1 - src files avoid direct getenv() calls"
