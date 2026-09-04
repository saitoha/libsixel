# Tokenize C source sufficiently for the filter component boundary checks.
# Comments and literals are discarded, while identifier and member-access
# tokens remain ordered across physical lines.

function clear_file_state(    name) {
    lexical_state = "code"
    escaped = 0
    token = ""
    token_line = 0
    previous1 = ""
    previous2 = ""
    previous3 = ""
    previous4 = ""
    filter_declaration = 0
    vtbl_candidate = 0
    vtbl_saw_equal = 0
    in_vtbl = 0
    vtbl_depth = 0
    vtbl_field = 0
    vtbl_apply_seen = 0
    for (name in filter_variables) {
        delete filter_variables[name]
    }
}

function file_basename(path, value) {
    value = path
    sub(/^.*\//, "", value)
    return value
}

function is_c_input(path) {
    return path ~ /\.c$/ || path ~ /\.c\.in$/
}

function is_h_input(path) {
    return path ~ /\.h$/ || path ~ /\.h\.in$/
}

function is_filter_source(path, name) {
    name = file_basename(path)
    sub(/\.in$/, "", name)
    return name ~ /^filter-.*\.c$/ && name != "filter-factory.c"
}

function is_owner(component, expected, owner_basename) {
    gsub(/_/, "-", component)
    expected = "filter-" component ".c"
    owner_basename = file_basename(FILENAME)
    sub(/\.in$/, "", owner_basename)
    return owner_basename == expected
}

function execution_owner(symbol, component) {
    component = symbol
    sub(/^sixel_filter_/, "", component)
    sub(/_(apply|frame)$/, "", component)
    sub(/_(copy|create)$/, "", component)
    if (component == "1d_eytzinger") {
        component = "eytzinger"
    }
    return component
}

function constructor_owner(symbol, component) {
    component = symbol
    sub(/^sixel_filter_/, "", component)
    sub(/_init$/, "", component)
    if (component == "1d_eytzinger") {
        component = "eytzinger"
    }
    return component
}

function emit_violation(category, line, detail) {
    print category "|" FILENAME "|" line "|" detail
}

function remember_filter_variable(value) {
    if (value ~ /^[A-Za-z_][A-Za-z0-9_]*$/ &&
            value != "const" && value != "volatile" &&
            value != "restrict") {
        filter_variables[value] = 1
        filter_declaration = 0
    } else if (value == ")" || value == ";" || value == "=") {
        filter_declaration = 0
    }
}

function check_vtbl_callback(value, line, component) {
    if (vtbl_apply_seen != 0 || vtbl_field != 2 ||
            value !~ /^[A-Za-z_][A-Za-z0-9_]*$/) {
        return
    }
    vtbl_apply_seen = 1
    if (value !~ /^sixel_filter_[A-Za-z0-9_]+_apply$/) {
        emit_violation("execution", line,
                       "nonstandard vtbl apply callback " value)
        return
    }
    component = execution_owner(value)
    if (!is_owner(component)) {
        emit_violation("execution", line,
                       "foreign vtbl apply callback " value)
    }
}

function track_vtbl(value, line) {
    if (in_vtbl != 0) {
        if (value == "{") {
            ++vtbl_depth
        } else if (value == "}") {
            --vtbl_depth
            if (vtbl_depth == 0) {
                in_vtbl = 0
                vtbl_candidate = 0
                vtbl_saw_equal = 0
            }
        } else if (value == "," && vtbl_depth == 1) {
            ++vtbl_field
        } else if (vtbl_depth == 1) {
            check_vtbl_callback(value, line)
        }
        return
    }

    if (value == "sixel_filter_vtbl_t") {
        vtbl_candidate = 1
        vtbl_saw_equal = 0
        return
    }
    if (vtbl_candidate == 0) {
        return
    }
    if (value == "=") {
        vtbl_saw_equal = 1
    } else if (value == "{" && vtbl_saw_equal != 0) {
        in_vtbl = 1
        vtbl_depth = 1
        vtbl_field = 0
        vtbl_apply_seen = 0
    } else if (value == ";") {
        vtbl_candidate = 0
        vtbl_saw_equal = 0
    }
}

function check_token(value, line, component, basename) {
    basename = file_basename(FILENAME)
    sub(/\.in$/, "", basename)

    track_vtbl(value, line)

    if (value == "sixel_filter_t") {
        filter_declaration = 1
    } else if (filter_declaration != 0) {
        remember_filter_variable(value)
    }

    if (is_h_input(FILENAME) &&
            value ~ /^sixel_filter_[A-Za-z0-9_]+_(apply|frame)$/) {
        emit_violation("header", line, "exposed execution symbol " value)
    }

    if (is_c_input(FILENAME) &&
            value ~ /^sixel_filter_[A-Za-z0-9_]+_(apply|frame)$/) {
        component = execution_owner(value)
        if (!is_owner(component)) {
            emit_violation("execution", line,
                           "foreign execution symbol " value)
        }
    }

    if (is_c_input(FILENAME) &&
            value ~ /^sixel_filter_[A-Za-z0-9_]+_init$/) {
        component = constructor_owner(value)
        if (basename != "filter-factory.c" && !is_owner(component)) {
            emit_violation("construction", line,
                           "foreign constructor " value)
        }
    }

    if (value == "sixel_filter_alloc" && basename != "filter.c" &&
            basename != "filter-factory.c") {
        emit_violation("construction", line,
                       "filter allocation bypasses factory")
    }
    if (value == "sixel_filter_init_with_vtbl" &&
            basename != "filter.c" && !is_filter_source(FILENAME)) {
        emit_violation("construction", line,
                       "vtbl initialization outside filter component")
    }
    if (value == "sixel_filter_init" && basename != "filter.c") {
        emit_violation("construction", line,
                       "generic filter initialization outside filter core")
    }

    if ((value == "prepare" || value == "validate" || value == "apply" ||
            value == "dispose") && previous1 == "->" &&
            previous2 == "vtbl" && previous3 == "->" &&
            (previous4 in filter_variables) && basename != "filter.c") {
        emit_violation("dispatch", line,
                       "direct filter vtbl method " value)
    }
    if ((value == "prepare" || value == "validate" || value == "apply" ||
            value == "dispose") && previous1 == "." &&
            previous2 == "vtbl" && previous3 == "." &&
            (previous4 in filter_variables) && basename != "filter.c") {
        emit_violation("dispatch", line,
                       "direct filter vtbl method " value)
    }

    previous4 = previous3
    previous3 = previous2
    previous2 = previous1
    previous1 = value
}

function flush_token() {
    if (token != "") {
        check_token(token, token_line)
        token = ""
        token_line = 0
    }
}

function line_needs_scan(line, name) {
    if (lexical_state != "code" || in_vtbl != 0 || vtbl_candidate != 0) {
        return 1
    }
    if (previous1 == "->" || previous1 == "." || previous1 == "vtbl" ||
            previous2 == "vtbl" || previous3 == "vtbl") {
        return 1
    }
    if (index(line, "sixel_filter") != 0 || index(line, "vtbl") != 0 ||
            index(line, "/*") != 0 || index(line, "//") != 0 ||
            index(line, "\"") != 0 || index(line, "'") != 0) {
        return 1
    }
    for (name in filter_variables) {
        if (index(line, name) != 0) {
            return 1
        }
    }
    return 0
}

FNR == 1 {
    clear_file_state()
}

{
    source_line = $0
    if (!line_needs_scan(source_line)) {
        next
    }
    for (column = 1; column <= length(source_line); ++column) {
        character = substr(source_line, column, 1)
        next_character = substr(source_line, column + 1, 1)

        if (lexical_state == "block-comment") {
            if (character == "*" && next_character == "/") {
                lexical_state = "code"
                ++column
            }
            continue
        }
        if (lexical_state == "string" || lexical_state == "character") {
            if (escaped != 0) {
                escaped = 0
            } else if (character == "\\") {
                escaped = 1
            } else if ((lexical_state == "string" && character == "\"") ||
                    (lexical_state == "character" && character == "'")) {
                lexical_state = "code"
            }
            continue
        }

        if (character == "/" && next_character == "*") {
            flush_token()
            lexical_state = "block-comment"
            ++column
            continue
        }
        if (character == "/" && next_character == "/") {
            flush_token()
            break
        }
        if (character == "\"") {
            flush_token()
            lexical_state = "string"
            escaped = 0
            continue
        }
        if (character == "'") {
            flush_token()
            lexical_state = "character"
            escaped = 0
            continue
        }
        if (character ~ /[A-Za-z0-9_]/) {
            if (token == "") {
                token_line = FNR
            }
            token = token character
            continue
        }

        flush_token()
        if (character == "-" && next_character == ">") {
            check_token("->", FNR)
            ++column
        } else if (character ~ /[{}(),=*.;]/) {
            check_token(character, FNR)
        }
    }
    flush_token()
}

END {
    flush_token()
}
