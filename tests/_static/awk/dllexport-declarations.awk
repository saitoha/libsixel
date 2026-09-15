# SPDX-License-Identifier: MIT
# Inspect top-level function declarations, preserving their storage class.
# Bodies are skipped, including calls inside static inline definitions. This
# is a declaration scanner, not a C preprocessor; conditional declarations
# are checked conservatively in every branch.
function start_header(name) {
    declaration = ""
    declaration_line = 0
    header = name
    body_depth = 0
    linkage_depth = 0
    block_comment = 0
    quote = ""
    escaped = 0
    directive = 0
}
function emit_declaration(    text, prefix, name, offset, location, exported) {
    text = declaration
    if (text ~ /(^|[[:space:]])typedef([[:space:]]|$)/ || text ~ /=/) {
        return
    }
    if (!match(text, /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)) {
        return
    }
    offset = RSTART
    name = substr(text, RSTART, RLENGTH)
    sub(/[[:space:]]*\($/, "", name)
    prefix = substr(text, 1, offset - 1)
    # Only storage class before the declarator counts: an array parameter
    # such as int values[static 3] does not give the function local linkage.
    if (prefix ~ /(^|[[:space:]])static([[:space:]]|$)/) {
        return
    }
    # compat_stub.h defines SIXEL_COMPAT_API as __declspec(dllexport).
    exported = prefix ~ /(^|[[:space:]])(SIXELAPI|SIXEL_INTERNAL_API|SIXEL_COMPAT_API)([[:space:]]|$)/
    location = declaration_line + gsub(/\n/, "\n", prefix)
    if (self_test) {
        observed[name] = exported
    } else {
        printf "%s\t%s:%d\t%d\n", name, header, location, exported
    }
}
function scan_line(line, number,    i, ch, next_ch, trimmed) {
    if (directive || line ~ /^[[:space:]]*#/) {
        directive = line ~ /\\[[:space:]]*$/
        return
    }
    for (i = 1; i <= length(line); ++i) {
        ch = substr(line, i, 1)
        next_ch = substr(line, i + 1, 1)
        if (block_comment) {
            if (ch == "*" && next_ch == "/") {
                block_comment = 0
                ++i
            }
            continue
        }
        if (quote != "") {
            if (!body_depth) {
                declaration = declaration ch
            }
            if (escaped) {
                escaped = 0
            } else if (ch == "\\") {
                escaped = 1
            } else if (ch == quote) {
                quote = ""
            }
            continue
        }
        if (ch == "/" && next_ch == "/") {
            break
        }
        if (ch == "/" && next_ch == "*") {
            block_comment = 1
            declaration = declaration " "
            ++i
            continue
        }
        if (ch == "\"" || ch == sprintf("%c", 39)) {
            quote = ch
        }
        if (body_depth) {
            if (ch == "{") {
                ++body_depth
            } else if (ch == "}") {
                --body_depth
            }
            continue
        }
        if (ch == "{") {
            trimmed = declaration
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", trimmed)
            if (trimmed ~ /^extern[[:space:]]+"C(\+\+)?"$/) {
                ++linkage_depth
            } else {
                emit_declaration()
                body_depth = 1
            }
            declaration = ""
            declaration_line = 0
        } else if (ch == ";" || ch == "}") {
            if (ch == ";") {
                emit_declaration()
            } else if (linkage_depth) {
                --linkage_depth
            }
            declaration = ""
            declaration_line = 0
        } else {
            if (!declaration_line && ch !~ /[[:space:]]/) {
                declaration_line = number
            }
            declaration = declaration ch
        }
    }
    if (declaration_line) {
        declaration = declaration "\n"
    }
}
BEGIN {
    # Literal controls exercise storage, declaration boundaries, bodies,
    # parameter qualifiers, C++ linkage wrappers, and independent headers.
    self_test = 1
    start_header("positive")
    scan_line("SIXELAPI int visible(void);", 1)
    scan_line("static inline int local(void) { visible(); hidden(); }", 2)
    scan_line("extern inline int external(void);", 3)
    scan_line("int parameter(int values[static 3]);", 4)
    scan_line("extern \"C\" {", 5)
    scan_line("SIXEL_INTERNAL_API int wrapped(void);", 6)
    scan_line("}", 7)
    scan_line("SIXEL_COMPAT_API int compatible(void);", 8)
    scan_line("SIXELAPI", 9)
    start_header("negative")
    scan_line("int unmarked(void);", 1)
    scan_line("SIXELAPI int first(void); int second(void);", 2)
    scan_line("static", 3)
    scan_line("inline int split(void) { hidden(); }", 4)
    if (!("visible" in observed) || observed["visible"] != 1 ||
        ("local" in observed) || ("hidden" in observed) ||
        !("external" in observed) || observed["external"] != 0 ||
        !("parameter" in observed) || observed["parameter"] != 0 ||
        observed["compatible"] != 1 || observed["wrapped"] != 1 || observed["unmarked"] != 0 ||
        !("unmarked" in observed) || !("second" in observed) ||
        observed["first"] != 1 || observed["second"] != 0 ||
        ("split" in observed)) {
        print "dllexport declaration scanner self-test failed" > "/dev/stderr"
        exit 1
    }
    self_test = 0
}
FNR == 1 { start_header(FILENAME) }
{ scan_line($0, FNR) }
