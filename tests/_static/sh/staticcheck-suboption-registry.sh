#!/bin/sh
# Emit TAP for the single suboption registry and its mandatory metadata.

set -eu

echo "1..1"

src_root=$1
registry_file=$src_root/src/options-registry.c
duplicates=

test -f "$registry_file" || {
    echo "not ok 1 - suboptions use one complete registry"
    echo "# missing src/options-registry.c"
    exit 1
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
    exit 1
}

awk '
function fail(message) {
    print "# " message
    failed = 1
}
function inspect(row, fields, count, option_id, base, name, alias, env,
                 exact_key, shared_key) {
    gsub(/[[:space:]]+/, " ", row)
    sub(/^.*SIXEL_REGISTRY_(CHOICE|FREE)\(/, "", row)
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
    if (env == "NULL" || env == "") {
        fail(option_id ":" name " needs an environment variable")
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
in_registry && /SIXEL_REGISTRY_(CHOICE|FREE)\(/ {
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
    exit 1
}

echo "ok 1 - suboptions use one complete registry"
exit 0
