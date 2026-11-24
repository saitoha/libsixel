#!/bin/sh
# Emit TAP for duplicate static functions, arrays and test types.

set -eu

echo "1..1"

src_root=$1
src_makefile=$src_root/src/Makefile.am
converters_makefile=$src_root/converters/Makefile.am
tests_makefile=$src_root/tests/Makefile.am

if test ! -f "$src_makefile"; then
    echo "ok 1 # SKIP missing src/Makefile.am"
    exit 0
fi
if test ! -f "$converters_makefile"; then
    echo "ok 1 # SKIP missing converters/Makefile.am"
    exit 0
fi
if test ! -f "$tests_makefile"; then
    echo "ok 1 # SKIP missing tests/Makefile.am"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-static-symbols-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

unit_entries=$tmpdir/unit_entries.txt
unit_files=$tmpdir/unit_files.txt
raw_symbols=$tmpdir/raw_symbols.tsv
symbols=$tmpdir/symbols.tsv
duplicates=$tmpdir/duplicates.tsv
amalgamation_defines=$tmpdir/amalgamation-defines.h
prefix_issues=$tmpdir/prefix-issues.log
: > "$prefix_issues"

awk -v src_base="$src_root/src" '
{
    line = $0
    sub(/#.*/, "", line)
    while (match(line, /\$\(srcdir\)\/[A-Za-z0-9_.\/-]+\.[cm]/)) {
        token = substr(line, RSTART, RLENGTH)
        sub(/^\$\(srcdir\)\//, "", token)
        print src_base "/" token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$src_makefile" > "$unit_entries"

awk -v src_base="$src_root/converters" '
{
    line = $0
    sub(/#.*/, "", line)
    while (match(line, /\$\(srcdir\)\/[A-Za-z0-9_.\/-]+\.[cm]/)) {
        token = substr(line, RSTART, RLENGTH)
        sub(/^\$\(srcdir\)\//, "", token)
        print src_base "/" token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$converters_makefile" >> "$unit_entries"

awk -v tests_base="$src_root/tests" -v top_base="$src_root" '
function remember(token) {
    if (token ~ /^\$\(top_srcdir\)\//) {
        sub(/^\$\(top_srcdir\)\//, "", token)
        print top_base "/" token
        return
    }
    if (token ~ /^\$\(srcdir\)\//) {
        sub(/^\$\(srcdir\)\//, "", token)
        print tests_base "/" token
        return
    }
    if (token !~ /^\$\(/) {
        print tests_base "/" token
    }
}
# Include test-runner convenience libraries with isolated compiler flags.
function scan_sources(line, token) {
    sub(/#.*/, "", line)
    while (match(line,
                 /(\$\(top_srcdir\)\/|\$\(srcdir\)\/)?[A-Za-z0-9_.\/-]+\.c/)) {
        token = substr(line, RSTART, RLENGTH)
        remember(token)
        line = substr(line, RSTART + RLENGTH)
    }
}
$0 ~ /^[[:space:]]*((test_runner|[[:alnum:]_]+_la)_SOURCES|batch_[[:alnum:]_]+)[[:space:]]*=/ {
    in_sources = 1
}
in_sources {
    scan_sources($0)
}
in_sources && $0 !~ /\\[[:space:]]*$/ {
    in_sources = 0
}
' "$tests_makefile" "$src_root/tests/runner-batches.am" >> "$unit_entries"

# The test runner amalgamation enables every registered C test in one
# translation unit.  Its private functions and typedef names therefore share
# the ordinary identifier namespace even when their source files differ.
# Resolve simple object-like renaming macros used by unity-safe tests.  Helpers
# under feature conditionals must remain unique as well because amalgamated CI
# configurations can enable several guarded tests in the same translation unit.
LC_ALL=C sort -u "$unit_entries" -o "$unit_entries"

if awk '
/SIXEL_AMALGAMATION_SPLIT_DITHER_POLICY=1/ {
    found = 1
}
END {
    exit found ? 0 : 1
}
' "$src_makefile"; then
    awk '
{
    is_dither_policy = (index($0, "/src/dither-policy-") > 0)
    is_c_file = ($0 ~ /\.c$/)
    if (!(is_dither_policy && is_c_file)) {
        print
    }
}
' "$unit_entries" > "$tmpdir/unit_entries.filtered"
    mv "$tmpdir/unit_entries.filtered" "$unit_entries"
fi

if test ! -s "$unit_entries"; then
    echo "ok 1 # SKIP no C/ObjC source entries found"
    exit 0
fi

: > "$unit_files"
while IFS= read -r unit_path; do
    test -n "$unit_path" || continue
    case "$unit_path" in *.c|*.m) ;; *) continue ;; esac
    test -f "$unit_path" || continue
    printf '%s\n' "$unit_path" >> "$unit_files"
done < "$unit_entries"

if test ! -s "$unit_files"; then
    echo "ok 1 # SKIP no C/ObjC amalgamation units found"
    exit 0
fi

# Use the same test selectors as the generated amalgamated runner.
awk -f "$src_root/build-aux/test-runner-amalgamation-defines.awk" \
    "$tests_makefile" > "$amalgamation_defines"

# Keep parsing state local to each translation unit while reusing one process.
# Loading argv from the inventory also avoids the host command-length limit.
awk -v units="$unit_files" -v tests_root="$src_root/tests/" \
    -v src_dir="$src_root/src/" -v defines="$amalgamation_defines" \
    -v prefix_issues="$prefix_issues" '
function add_unit(path,    line, fragment) {
    if (path in scanned) return
    scanned[path] = 1
    ARGV[ARGC++] = path
    if (index(path, src_dir) != 1) return
    # Source fragments share the amalgamation namespace with their owners.
    # Ignore system and generated headers, which are outside this inventory.
    while ((getline line < path) > 0) {
        if (line !~ /^[[:space:]]*#[[:space:]]*include[[:space:]]+"[A-Za-z0-9_.-]+\.inc\.h"/) continue
        sub(/^[^"]*"/, "", line)
        sub(/".*$/, "", line)
        fragment = src_dir line
        add_unit(fragment)
    }
    close(path)
}
BEGIN {
    amalgamation_macro["SIXEL_AMALGAMATION"] = 1
    while ((getline line < defines) > 0) {
        split(line, definition, " ")
        amalgamation_macro[definition[2]] = 1
    }
    close(defines)
    while ((getline file < units) > 0) add_unit(file)
    close(units)
}
function trim(s) {
    sub(/^[[:space:]]+/, "", s)
    sub(/[[:space:]]+$/, "", s)
    return s
}
function strip_comments(s,   out, seg, p0, p1) {
    out = ""
    seg = s
    while (1) {
        if (in_comment) {
            p1 = index(seg, "*/")
            if (p1 == 0) {
                return out
            }
            seg = substr(seg, p1 + 2)
            in_comment = 0
            continue
        }
        p0 = index(seg, "/*")
        p1 = index(seg, "//")
        if (p1 > 0 && (p0 == 0 || p1 < p0)) {
            out = out substr(seg, 1, p1 - 1)
            return out
        }
        if (p0 == 0) {
            out = out seg
            return out
        }
        out = out substr(seg, 1, p0 - 1)
        seg = substr(seg, p0 + 2)
        in_comment = 1
    }
}
function count_open(s, t) {
    t = s
    return gsub(/\(/, "", t)
}
function count_close(s, t) {
    t = s
    return gsub(/\)/, "", t)
}
function count_brace_open(s, t) {
    t = s
    return gsub(/\{/, "", t)
}
function count_brace_close(s, t) {
    t = s
    return gsub(/\}/, "", t)
}
function emit_symbol(name, namespace, resolved) {
    resolved = (name in macro_alias) ? macro_alias[name] : name
    if (namespace == "tag") {
        resolved = "tag:" resolved
    }
    if (index(file, src_dir) == 1 && namespace == "ordinary") {
        source_symbols[file, ++source_count[file]] = name
    }
    # Alternate backends may export the same function. Only compare an
    # exported function against a test that includes its own source file.
    if (external_function) return
    printf "%s\t%s\n", resolved, file
}
# Evaluate the simple guards used around sources that are built separately.
# Other preprocessor conditions are unknown and leave an include eligible.
function guard_value(line,  macro) {
    macro = line
    gsub(/[[:space:]()]/, "", macro)
    if (sub(/^#(ifndef|if!defined|elif!defined)/, "", macro)) {
        return macro in amalgamation_macro ? -1 : 0
    }
    if (sub(/^#(ifdef|ifdefined|elifdefined)/, "", macro)) {
        return macro in amalgamation_macro ? 1 : 0
    }
    return 0
}
function track_guard(line,  value) {
    if (line ~ /^#[[:space:]]*if(n?def)?[[:space:](]/) {
        ++if_depth
        value = guard_value(line)
    } else if (line ~ /^#[[:space:]]*(elif|else|endif)([[:space:](]|$)/) {
        if (if_depth in excluded_at) {
            delete excluded_at[if_depth]
            --excluded
        }
        if (line ~ /^#[[:space:]]*endif([[:space:]]|$)/) {
            delete taken_at[if_depth]
            --if_depth
            return
        }
        value = if_depth in taken_at ? -1 : guard_value(line)
    } else {
        return
    }
    if (value > 0) {
        taken_at[if_depth] = 1
    } else if (value < 0) {
        excluded_at[if_depth] = 1
        ++excluded
    }
}
function emit_source_copy(line,  source, i, original, alias) {
    source = line
    sub(/^[^"]*"src\//, src_dir, source)
    sub(/".*$/, "", source)
    # The merged source keeps these includes intact. Account for every
    # definition again using the aliases in scope at the include.
    for (i = 1; i <= source_count[source]; ++i) {
        original = source_symbols[source, i]
        alias = (original in macro_alias) ? macro_alias[original] : original
        printf "%s\t%s\n", original, source
        printf "%s\t%s:%d\n", alias, file, FNR
    }
}
function emit_typedef(text,   name, parts, rest, tag, part_count) {
    if (!is_test) {
        return
    }
    rest = text
    if (rest ~ /^typedef[[:space:]]+(struct|union|enum)[[:space:]]+/) {
        sub(/^typedef[[:space:]]+(struct|union|enum)[[:space:]]+/, "",
            rest)
        tag = rest
        sub(/[^A-Za-z0-9_].*$/, "", tag)
        if (tag ~ /^[A-Za-z_][A-Za-z0-9_]*$/) {
            emit_symbol(tag, "tag")
        }
    }

    rest = text
    if (rest ~ /\(\*[[:space:]]*[A-Za-z_][A-Za-z0-9_]*/) {
        sub(/^.*\(\*[[:space:]]*/, "", rest)
        name = rest
        sub(/[^A-Za-z0-9_].*$/, "", name)
    } else {
        sub(/[[:space:]]*;[[:space:]]*$/, "", rest)
        gsub(/[^A-Za-z0-9_]+/, " ", rest)
        sub(/^[[:space:]]+/, "", rest)
        sub(/[[:space:]]+$/, "", rest)
        part_count = split(rest, parts, /[[:space:]]+/)
        name = parts[part_count]
    }
    if (name ~ /^[A-Za-z_][A-Za-z0-9_]*$/) {
        emit_symbol(name, "ordinary")
    }
}
function record_variable(name) {
    if (is_test) test_variables[FNR, name] = 1
    emit_symbol(name, "ordinary")
}
# A test that compiles a private copy of production code needs its own
# namespace for file-scope state, including fixture data and callbacks.
# A variable can use the namespace itself or start with the namespace
# followed by an underscore.
function check_test_prefix(   variable, parts, required, namespace) {
    if (!copies_source && test_prefix == "") return
    required = test_prefix == "" ? "(add a Test variable prefix comment)" : test_prefix
    namespace = substr(test_prefix, 1, length(test_prefix) - 1)
    for (variable in test_variables) {
        split(variable, parts, SUBSEP)
        if (test_prefix == "" ||
                (parts[2] != namespace && index(parts[2], test_prefix) != 1)) {
            printf "# %s:%s: variable %s needs the test prefix %s\n", \
                file, parts[1], parts[2], required > prefix_issues
        }
    }
}
function emit_variable(line,  declaration) {
    declaration = line
    sub(/[\[=;].*$/, "", declaration)
    if (declaration !~ /[[:space:]*][A-Za-z_][A-Za-z0-9_]*[[:space:]]*$/ ||
            declaration ~ /[({]/) {
        return
    }
    sub(/[[:space:]]+$/, "", declaration)
    match(declaration, /[A-Za-z_][A-Za-z0-9_]*$/)
    record_variable(substr(declaration, RSTART, RLENGTH))
}
function reset_state() {
    state = 0
    external_function = 0
    name = ""
    depth = 0
}
function is_declaration_keyword(candidate) {
    return candidate == "char" || candidate == "double" ||
           candidate == "float" || candidate == "int" ||
           candidate == "long" || candidate == "short" ||
           candidate == "signed" || candidate == "unsigned" ||
           candidate == "void" || candidate == "_Bool" ||
           candidate == "__attribute__"
}
function start_signature(line,   candidate, search) {
    search = line
    while (match(search,
                 /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)) {
        candidate = substr(search, RSTART, RLENGTH)
        sub(/[[:space:]]*\($/, "", candidate)
        if (!is_declaration_keyword(candidate)) {
            name = candidate
            break
        }
        search = substr(search, RSTART + RLENGTH)
    }
    if (name == "") {
        return 0
    }
    depth = count_open(line) - count_close(line)
    if (line ~ /;[[:space:]]*$/ && depth <= 0) {
        reset_state()
        return 0
    }
    if (depth <= 0) {
        if (line ~ /\{/) {
            emit_symbol(name, "ordinary")
            reset_state()
            return 0
        }
        state = 3
        return 1
    }
    state = 2
    return 1
}
FNR == 1 {
    check_test_prefix()
    file = FILENAME
    delete test_variables
    test_prefix = ""
    copies_source = 0
    aggregate = 0
    delete macro_alias
    delete excluded_at
    delete taken_at
    excluded = 0
    if_depth = 0
    define_text = ""
    in_comment = 0
    is_test = index(file, tests_root) == 1
    in_typedef = 0
    typedef_depth = 0
    typedef_text = ""
    reset_state()
}
{
    if (is_test && $0 ~ /^\/\* Test variable prefix: [a-z][a-z0-9][a-z0-9_]*_ \*\/$/) {
        prefix = $0
        sub(/^\/\* Test variable prefix: /, "", prefix)
        sub(/ .*/, "", prefix)
        if (test_prefix != "" || prefix in prefix_owner) {
            print "# " file ":" FNR ": duplicate test variable prefix " prefix > prefix_issues
        }
        test_prefix = prefix
        prefix_owner[prefix] = file
    }
    line = trim(strip_comments($0))
    if (line == "" && define_text == "") {
        next
    }

    if (is_test && define_text == "" && line ~ /^#/) {
        track_guard(line)
    }
    if (is_test && line ~ /^#[[:space:]]*include[[:space:]]+"src\/[^"]+\.c"$/) {
        copies_source = 1
        if (!excluded) emit_source_copy(line)
    }
    if (excluded) next

    if (define_text != "" || line ~ /^#[[:space:]]*define[[:space:]]+/) {
        define_text = define_text == "" ? line : define_text " " line
        if (sub(/\\$/, "", define_text)) {
            next
        }
        macro = define_text
        define_text = ""
        sub(/^#[[:space:]]*define[[:space:]]+/, "", macro)
        if (macro ~ /^[A-Za-z_][A-Za-z0-9_]*[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*$/) {
            split(macro, macro_parts, /[[:space:]]+/)
            macro_alias[macro_parts[1]] = macro_parts[2]
        }
        next
    }
    if (line ~ /^#[[:space:]]*undef[[:space:]]+/) {
        macro = line
        sub(/^#[[:space:]]*undef[[:space:]]+/, "", macro)
        sub(/[[:space:]].*$/, "", macro)
        delete macro_alias[macro]
        next
    }
    if (line ~ /^#/) {
        next
    }

    if (in_typedef) {
        typedef_text = typedef_text " " line
        typedef_depth += count_brace_open(line) - count_brace_close(line)
        if (typedef_depth == 0 && line ~ /;/) {
            emit_typedef(typedef_text)
            in_typedef = 0
            typedef_text = ""
        }
        next
    }
    if (line ~ /^typedef([[:space:]]|$)/) {
        typedef_text = line
        typedef_depth = count_brace_open(line) - count_brace_close(line)
        if (typedef_depth == 0 && line ~ /;/) {
            emit_typedef(typedef_text)
            typedef_text = ""
        } else {
            in_typedef = 1
        }
        next
    }

    if (aggregate) {
        if ($0 ~ /^}/) {
            aggregate = 0
            if (line ~ /^}[[:space:]]+/) emit_variable("static" substr(line, 2))
        }
        next
    }

    if (state == 0) {
        if (index(file, src_dir) == 1 &&
                $0 ~ /^[A-Za-z][A-Za-z0-9_]*\(/) {
            external_function = 1
            start_signature(line)
            next
        }
        if (is_test && $0 ~ /^[A-Za-z_]/) {
            if (line ~ /^[^(]*\(\*/ &&
                    match(line, /\(\*[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\)/)) {
                pointer = substr(line, RSTART + 2, RLENGTH - 3)
                record_variable(trim(pointer))
                next
            }
            if (line ~ /^(static[[:space:]]+)?(struct|union|enum)[^=]*\{/) {
                if (sub(/^[^}]*}/, "", line)) {
                    if (line ~ /^[[:space:]]+/) emit_variable("static" line)
                } else {
                    aggregate = 1
                }
                next
            }
            if (line ~ /^[^(]*[\[=;]/ &&
                    line !~ /^(extern[[:space:]]+|(struct|union|enum)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*;)/) {
                emit_variable(line)
                next
            }
        }
        if ($0 ~ /^static([[:space:]]|$)/) {
            if (line ~ /^[^(]*[\[=;]/) {
                # Converter sources are built as separate frontends.
                if (index(file, src_dir) == 1) emit_variable(line)
                next
            }
            if (sub(/^static([[:space:]]+inline)?[[:space:]]+/, "", line)) {
                if (line ~ /^[^(]*[=\[]/ ||
                    (line ~ /\{/ && line !~ /\(/)) {
                    reset_state()
                    next
                }
                if (start_signature(line)) {
                    next
                }
            }
            state = 1
        }
        next
    }

    if (state == 1) {
        if (line ~ /^[^(]*[=\[]/ ||
            (line ~ /\{/ && line !~ /\(/)) {
            reset_state()
            next
        }
        if (line ~ /;[[:space:]]*$/ && line !~ /\(/) {
            reset_state()
            next
        }
        if (start_signature(line)) {
            next
        }
        next
    }

    if (state == 2) {
        depth += count_open(line) - count_close(line)
        if (depth > 0) {
            next
        }
        if (line ~ /;[[:space:]]*$/) {
            reset_state()
            next
        }
        if (line ~ /\{/) {
            emit_symbol(name, "ordinary")
            reset_state()
            next
        }
        state = 3
        next
    }

    if (state == 3) {
        if (line ~ /;[[:space:]]*$/) {
            reset_state()
            next
        }
        if (line ~ /\{/) {
            emit_symbol(name, "ordinary")
            reset_state()
            next
        }
    }
}
END { check_test_prefix() }
' > "$raw_symbols"

if test -s "$prefix_issues"; then
    cat "$prefix_issues"
    echo "not ok 1 - test source copies isolate their file-scope variables"
    exit 0
fi

if test ! -s "$raw_symbols"; then
    echo "ok 1 - amalgamation static symbols and test types are unique"
    exit 0
fi

LC_ALL=C sort -u "$raw_symbols" > "$symbols"

if awk -F '\t' '
{
    symbol = $1
    file = $2
    if (!(symbol in order)) {
        count += 1
        order[symbol] = count
        names[count] = symbol
        files[symbol] = file
        seen[symbol SUBSEP file] = 1
        symbol_count[symbol] = 1
        next
    }
    if ((symbol SUBSEP file) in seen) {
        next
    }
    seen[symbol SUBSEP file] = 1
    symbol_count[symbol] += 1
    files[symbol] = files[symbol] ", " file
}
END {
    status = 0
    for (i = 1; i <= count; ++i) {
        symbol = names[i]
        if (symbol_count[symbol] > 1) {
            status = 1
            printf "%s\t%d\t%s\n", symbol, symbol_count[symbol], files[symbol]
        }
    }
    exit status
}
' "$symbols" > "$duplicates"; then
    echo "ok 1 - amalgamation static symbols and test types are unique"
    exit 0
fi

echo "not ok 1 - amalgamation static symbols and test types are unique"
while IFS=$(printf '\t') read -r symbol count files; do
    test -n "$symbol" || continue
    echo "# duplicate static symbol or test type: $symbol ($count definitions)"
    echo "# files: $files"
done < "$duplicates"
exit 0
