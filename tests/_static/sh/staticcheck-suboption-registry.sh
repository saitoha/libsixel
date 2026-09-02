#!/bin/sh
# Emit TAP for the single suboption registry and its mandatory metadata.

set -eux

echo "1..1"
set -v

src_root=$1
registry_file=$src_root/src/options-registry.c
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
    if (env == "NULL" || env == "") {
        fail(option_id ":" name " needs an environment variable")
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
function inspect_registry(row, fields, count, name, alias, key) {
    gsub(/[[:space:]]+/, " ", row)
    sub(/^.*SIXEL_REGISTRY_[A-Z0-9_]+\(/, "", row)
    sub(/^[[:space:]]*/, "", row)
    sub(/\),[[:space:]]*$/, "", row)
    count = split(row, fields, /,[[:space:]]*/)
    if (count < 7) {
        fail("malformed registry row in image coverage check: " row)
        return
    }
    name = fields[3]
    alias = fields[4]
    gsub(/^"|"$/, "", name)
    gsub(/^\047|\047$/, "", alias)
    key = fields[1] "|" fields[2] "|" name
    expected[key] = 1
    expected_alias[key] = alias
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
FILENAME != registry_file {
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
    if (index($0, "--env") > 0) {
        has_environment[FILENAME] = 1
    }
    if (index($0, ":" expected_alias[key]) > 0) {
        has_short[FILENAME] = 1
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
        if (!has_lsqa[file]) {
            fail(file " does not validate decoded image quality")
        }
        if (!has_compare[file]) {
            fail(file " does not compare short and environment output")
        }
        if (!has_environment[file]) {
            fail(file " does not exercise the environment spelling")
        }
        if (!has_short[file]) {
            fail(file " does not exercise the registered short name")
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
