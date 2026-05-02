#!/usr/bin/awk -f
# Generate include/6cells.h from include/6cells.idl.

BEGIN {
    mode = ""
    has_error = 0
    emit_header()
}

function report_error(message) {
    printf("%s:%d: %s\n", FILENAME, FNR, message) > "/dev/stderr"
    has_error = 1
}

function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}

function last_index(text, needle,    i, pos) {
    pos = 0
    for (i = 1; i <= length(text); ++i) {
        if (substr(text, i, length(needle)) == needle) {
            pos = i
        }
    }
    return pos
}

function c_symbol(name) {
    if (name ~ /^sixel_/) {
        return name
    }
    return "sixel_" name
}

function c_type_alias(name) {
    if (name ~ /^sixel_/) {
        return name
    }
    return "sixel_" name "_t"
}

function c_struct_tag(name) {
    return c_symbol(name)
}

function c_interface_tag(name) {
    if (name ~ /^sixel_.*_interface$/) {
        return name
    }
    return c_symbol(name) "_interface"
}

function c_interface_vtbl(name) {
    if (name ~ /^sixel_/) {
        return name "_vtbl_t"
    }
    return c_symbol(name) "_vtbl_t"
}

function default_receiver(name,    parts, count) {
    count = split(name, parts, /_/)
    if (count > 0) {
        return parts[count]
    }
    return name
}

function clear_args(    i) {
    for (i = 1; i <= arg_count; ++i) {
        delete args[i]
    }
    arg_count = 0
}

function clear_enum(    i) {
    for (i = 1; i <= enum_count; ++i) {
        delete enum_entries[i]
    }
    enum_count = 0
}

function clear_methods(    i, j, count) {
    for (i = 1; i <= method_count; ++i) {
        count = method_arg_count[i]
        for (j = 1; j <= count; ++j) {
            delete method_args[i, j]
        }
        delete method_names[i]
        delete method_returns[i]
        delete method_arg_count[i]
    }
    method_count = 0
}

function clear_pending_attrs() {
    pending_alias = ""
    pending_receiver = ""
    pending_classid = ""
    pending_serviceid = ""
    pending_native = 0
    pending_const = 0
}

function clear_pending_idl_attrs(    i) {
    for (i = 1; i <= pending_idl_attr_count; ++i) {
        delete pending_idl_attrs[i]
    }
    pending_idl_attr_count = 0
}

function clear_idl_contract(    i) {
    for (i = 1; i <= idl_contract_count; ++i) {
        delete idl_contract_lines[i]
    }
    idl_contract_count = 0
}

function append_idl_contract(line) {
    idl_contract_lines[++idl_contract_count] = line
}

function strip_quotes(text) {
    text = trim(text)
    if (text ~ /^".*"$/) {
        text = substr(text, 2, length(text) - 2)
    }
    return text
}

function extract_attr_args(line, attr_name,    start, rest, depth, i, ch) {
    start = index(line, attr_name "(")
    if (start == 0) {
        return ""
    }
    rest = substr(line, start + length(attr_name) + 1)
    depth = 1
    for (i = 1; i <= length(rest); ++i) {
        ch = substr(rest, i, 1)
        if (ch == "(") {
            ++depth
        } else if (ch == ")") {
            --depth
            if (depth == 0) {
                return substr(rest, 1, i - 1)
            }
        }
    }
    return ""
}

function emit_contract_summary(    i, args, j, printed_forbid) {
    for (i = 1; i <= idl_contract_count; ++i) {
        args = extract_attr_args(idl_contract_lines[i], "responsibility")
        if (args != "") {
            print "/*"
            print " * IDL responsibility:"
            print " * - " strip_quotes(args)
            print " */"
            print ""
        }
    }
    printed_forbid = 0
    for (i = 1; i <= idl_contract_count; ++i) {
        args = extract_attr_args(idl_contract_lines[i], "forbid_state")
        if (args != "") {
            if (printed_forbid == 0) {
                print "/*"
                print " * IDL forbidden state:"
                printed_forbid = 1
            }
            split_attrs(args)
            for (j = 1; j <= attr_item_count; ++j) {
                print " * - " strip_quotes(attr_items[j])
            }
        }
    }
    if (printed_forbid != 0) {
        print " */"
        print ""
    }
}

function emit_idl_contract(    i) {
    if (idl_contract_count == 0) {
        return
    }
    emit_contract_summary()
    print "/*"
    print " * IDL contract:"
    for (i = 1; i <= idl_contract_count; ++i) {
        print " * " idl_contract_lines[i]
    }
    print " */"
    print ""
}

function emit_coclass_contract(    i) {
    emit_contract_summary()
    print "/*"
    print " * IDL coclass:"
    for (i = 1; i <= idl_contract_count; ++i) {
        print " * " idl_contract_lines[i]
    }
    print " */"
    print ""
    clear_pending_attrs()
    clear_pending_idl_attrs()
    clear_idl_contract()
}

function emit_header() {
    print "/*"
    print " * SPDX-License-Identifier: MIT"
    print " *"
    print " * Copyright (c) 2026 libsixel developers. See `AUTHORS`."
    print " *"
    print " * Permission is hereby granted, free of charge, to any person"
    print " * obtaining a copy of this software and associated documentation files"
    print " * (the \"Software\"), to deal in the Software without restriction,"
    print " * including without limitation the rights to use, copy, modify, merge,"
    print " * publish, distribute, sublicense, and/or sell copies of the Software,"
    print " * and to permit persons to whom the Software is furnished to do so,"
    print " * subject to the following conditions:"
    print " *"
    print " * The above copyright notice and this permission notice shall be"
    print " * included in all copies or substantial portions of the Software."
    print " *"
    print " * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,"
    print " * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF"
    print " * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT."
    print " * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY"
    print " * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,"
    print " * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE"
    print " * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE."
    print " */"
    print ""
    print "/*"
    print " * Auto-generated by tools/gen_6cells_h.awk from include/6cells.idl;"
    print " * do not edit include/6cells.h directly."
    print " */"
    print ""
    print "#ifndef LIBSIXEL_6CELLS_H"
    print "#define LIBSIXEL_6CELLS_H"
    print ""
    print "#include <stddef.h>"
    print ""
    print "#include <sixel.h>"
    print ""
    print "#ifdef __cplusplus"
    print "extern \"C\" {"
    print "#endif"
    print ""
}

function emit_footer() {
    print "#ifdef __cplusplus"
    print "}"
    print "#endif"
    print ""
    print "#endif /* LIBSIXEL_6CELLS_H */"
    print ""
    print "/* emacs Local Variables:      */"
    print "/* emacs mode: c               */"
    print "/* emacs tab-width: 4          */"
    print "/* emacs indent-tabs-mode: nil */"
    print "/* emacs c-basic-offset: 4     */"
    print "/* emacs End:                  */"
    print "/* vim: set expandtab ts=4 sts=4 sw=4 : */"
    print "/* EOF */"
}

function emit_args(prefix, close_text,    i) {
    if (arg_count == 0) {
        print prefix "void" close_text
        return
    }
    if (arg_count == 1) {
        print prefix args[1] close_text
        return
    }
    print prefix args[1] ","
    for (i = 2; i < arg_count; ++i) {
        print "    " args[i] ","
    }
    print "    " args[arg_count] close_text
}

function pointer_separator(return_type) {
    return (return_type ~ /\*$/) ? "" : " "
}

function emit_struct_typedef(tag, alias,    line) {
    line = "typedef struct " tag " " alias ";"
    if (length(line) <= 78) {
        print line
    } else {
        print "typedef struct " tag
        print "    " alias ";"
    }
    print ""
}

function emit_function(return_type, name) {
    print return_type
    emit_args(name "(", ");")
    print ""
}

function emit_callback(return_type, name,    prefix, i, one_line) {
    prefix = "typedef " return_type pointer_separator(return_type) \
        "(*" name ")("
    if (arg_count == 0) {
        print prefix "void);"
        print ""
        return
    }
    if (arg_count == 1) {
        one_line = prefix args[1] ");"
        if (length(one_line) <= 78) {
            print one_line
            print ""
            return
        }
    }
    print prefix
    for (i = 1; i < arg_count; ++i) {
        print "    " args[i] ","
    }
    print "    " args[arg_count] ");"
    print ""
}

function emit_method(method_index,    ret, name, count, i, one_line, pointer) {
    ret = method_returns[method_index]
    name = method_names[method_index]
    count = method_arg_count[method_index]
    pointer = ret pointer_separator(ret) "(*" name ")"
    if (count == 0) {
        print "    " pointer "(void);"
        return
    }
    if (count == 1) {
        one_line = "    " pointer "(" method_args[method_index, 1] ");"
        if (length(one_line) <= 78) {
            print one_line
            return
        }
    }
    if (length(ret) > 32) {
        print "    " ret
        print "    (*" name ")("
    } else {
        print "    " pointer "("
    }
    for (i = 1; i < count; ++i) {
        print "        " method_args[method_index, i] ","
    }
    print "        " method_args[method_index, count] ");"
}

function emit_interface(    i, vtbl_tag) {
    if (forwarded_interface[interface_name] != 1) {
        emit_struct_typedef(interface_tag, interface_alias)
    }
    emit_idl_contract()
    vtbl_tag = interface_vtbl
    sub(/_t$/, "", vtbl_tag)
    printf("typedef struct %s {\n", vtbl_tag)
    for (i = 1; i <= method_count; ++i) {
        emit_method(i)
    }
    printf("} %s;\n", interface_vtbl)
    print ""
    printf("struct %s {\n", interface_tag)
    printf("    %s const *vtbl;\n", interface_vtbl)
    print "};"
    print ""
    clear_methods()
    clear_idl_contract()
}

function resolve_known_types(text,    out, i, c, token) {
    out = ""
    i = 1
    while (i <= length(text)) {
        c = substr(text, i, 1)
        if (c ~ /[A-Za-z_]/) {
            token = c
            ++i
            while (i <= length(text) &&
                    substr(text, i, 1) ~ /[A-Za-z0-9_]/) {
                token = token substr(text, i, 1)
                ++i
            }
            if (token in known_type) {
                out = out known_type[token]
            } else {
                out = out token
            }
        } else {
            out = out c
            ++i
        }
    }
    return out
}

function resolve_declarator(text,    name, prefix) {
    text = trim(text)
    if (!match(text, /[A-Za-z_][A-Za-z0-9_]*$/)) {
        return resolve_known_types(text)
    }
    name = substr(text, RSTART, RLENGTH)
    prefix = substr(text, 1, RSTART - 1)
    return resolve_known_types(prefix) name
}

function strip_direction(argument) {
    argument = trim(argument)
    sub(/^(in|out|inout)[ \t]+/, "", argument)
    return argument
}

function append_argument(argument) {
    argument = trim(argument)
    if (argument == "" || argument == "void") {
        return
    }
    args[++arg_count] = resolve_declarator(strip_direction(argument))
}

function parse_arguments(arguments,    parts, count, i) {
    clear_args()
    arguments = trim(arguments)
    if (arguments == "" || arguments == "void") {
        return
    }
    count = split(arguments, parts, /,/)
    for (i = 1; i <= count; ++i) {
        append_argument(parts[i])
    }
}

function split_attrs(attrs,    i, ch, token, depth, in_quote, prev) {
    for (i = 1; i <= attr_item_count; ++i) {
        delete attr_items[i]
    }
    attr_item_count = 0
    token = ""
    depth = 0
    in_quote = 0
    prev = ""
    for (i = 1; i <= length(attrs); ++i) {
        ch = substr(attrs, i, 1)
        if (ch == "\"" && prev != "\\") {
            in_quote = in_quote == 0 ? 1 : 0
        }
        if (in_quote == 0) {
            if (ch == "(") {
                ++depth
            } else if (ch == ")" && depth > 0) {
                --depth
            } else if (ch == "," && depth == 0) {
                attr_items[++attr_item_count] = trim(token)
                token = ""
                prev = ch
                continue
            }
        }
        token = token ch
        prev = ch
    }
    if (trim(token) != "") {
        attr_items[++attr_item_count] = trim(token)
    }
}

function is_contract_attr(item) {
    return item == "component" ||
        item == "native" ||
        item == "opaque" ||
        item == "refcounted" ||
        item == "mutates" ||
        item == "default" ||
        item ~ /^lifetime\(/ ||
        item ~ /^invalidates\(/ ||
        item ~ /^borrows\(/ ||
        item ~ /^responsibility\(/ ||
        item ~ /^forbid_state\(/ ||
        item ~ /^serviceid\(/
}

function parse_attrs(attrs,    count, i, item, value) {
    pending_idl_attrs[++pending_idl_attr_count] = "[" attrs "]"
    split_attrs(attrs)
    count = attr_item_count
    for (i = 1; i <= count; ++i) {
        item = attr_items[i]
        if (item == "native") {
            pending_native = 1
        } else if (item == "const") {
            pending_const = 1
        } else if (item ~ /^alias\(/ && item ~ /\)$/) {
            value = item
            sub(/^alias\(/, "", value)
            sub(/\)$/, "", value)
            pending_alias = trim(value)
        } else if (item ~ /^receiver\(/ && item ~ /\)$/) {
            value = item
            sub(/^receiver\(/, "", value)
            sub(/\)$/, "", value)
            pending_receiver = trim(value)
        } else if (item ~ /^classid\(/ && item ~ /\)$/) {
            value = item
            sub(/^classid\(/, "", value)
            sub(/\)$/, "", value)
            pending_classid = trim(value)
        } else if (item ~ /^serviceid\(/ && item ~ /\)$/) {
            value = item
            sub(/^serviceid\(/, "", value)
            sub(/\)$/, "", value)
            pending_serviceid = trim(value)
        } else if (is_contract_attr(item)) {
            continue
        } else {
            report_error("unknown attribute: " item)
        }
    }
}

function take_attrs(line,    close_pos, attrs) {
    while (line ~ /^\[[^]]+\]/) {
        close_pos = index(line, "]")
        attrs = substr(line, 2, close_pos - 2)
        parse_attrs(attrs)
        line = trim(substr(line, close_pos + 1))
    }
    return line
}

function setup_interface(name,    alias, receiver) {
    alias = pending_alias
    if (alias == "" && (name in interface_aliases)) {
        alias = interface_aliases[name]
    }
    if (alias == "") {
        alias = c_interface_tag(name) "_t"
    }
    receiver = pending_receiver
    if (receiver == "" && (name in interface_receivers)) {
        receiver = interface_receivers[name]
    }
    if (receiver == "") {
        receiver = default_receiver(name)
    }

    interface_name = name
    interface_tag = c_interface_tag(name)
    interface_alias = alias
    interface_vtbl = c_interface_vtbl(name)
    interface_receiver = receiver

    interface_aliases[name] = alias
    interface_receivers[name] = receiver
    known_type[name] = alias
    known_type[name "_vtbl"] = interface_vtbl
    clear_pending_attrs()
}

function emit_interface_forward(name) {
    setup_interface(name)
    emit_struct_typedef(interface_tag, interface_alias)
    forwarded_interface[name] = 1
    clear_pending_idl_attrs()
}

function register_struct_type(name) {
    record_tag = c_struct_tag(name)
    record_alias = record_tag "_t"
    known_type[name] = record_alias
}

function register_enum_type(name) {
    enum_tag = c_struct_tag(name)
    enum_alias = enum_tag "_t"
    known_type[name] = enum_alias
}

function emit_struct_field(line,    comment, field) {
    line = trim(line)
    comment = ""
    if (match(line, /[ \t]+\/\*.*\*\/$/)) {
        comment = substr(line, RSTART + 1)
        line = trim(substr(line, 1, RSTART - 1))
    }
    sub(/;$/, "", line)
    field = resolve_declarator(line)
    if (comment != "") {
        print "    " field "; " comment
    } else {
        print "    " field ";"
    }
}

function finish_record() {
    printf("} %s;\n", record_alias)
    print ""
}

function finish_enum(    i, entry, suffix) {
    for (i = 1; i <= enum_count; ++i) {
        entry = enum_entries[i]
        suffix = (i < enum_count) ? "," : ""
        print "    " entry suffix
    }
    printf("} %s;\n", enum_alias)
    print ""
    clear_enum()
}

function begin_decl(kind, line, after_mode, const_method) {
    decl_kind = kind
    decl_after_mode = after_mode
    decl_const = const_method
    decl_text = trim(line)
    if (decl_text ~ /\)[ \t]*(const[ \t]*)?;$/) {
        finish_decl()
    } else {
        mode = "decl"
    }
}

function parse_callable_decl(decl, kind, const_method,
                             open_pos, close_pos, before, arguments,
                             name, return_type, receiver_type, i) {
    sub(/[ \t]*;[ \t]*$/, "", decl)
    if (decl ~ /\)[ \t]*const$/) {
        const_method = 1
        sub(/[ \t]*const$/, "", decl)
    }

    open_pos = index(decl, "(")
    close_pos = last_index(decl, ")")
    if (open_pos == 0 || close_pos < open_pos) {
        report_error("invalid declaration: " decl)
        return
    }

    before = trim(substr(decl, 1, open_pos - 1))
    arguments = substr(decl, open_pos + 1, close_pos - open_pos - 1)
    if (!match(before, /[A-Za-z_][A-Za-z0-9_]*$/)) {
        report_error("missing callable name: " decl)
        return
    }

    name = substr(before, RSTART, RLENGTH)
    return_type = trim(substr(before, 1, RSTART - 1))
    return_type = resolve_known_types(return_type)

    parse_arguments(arguments)
    if (kind == "method") {
        if (const_method) {
            receiver_type = interface_alias " const *" interface_receiver
        } else {
            receiver_type = interface_alias " *" interface_receiver
        }
        for (i = arg_count; i >= 1; --i) {
            args[i + 1] = args[i]
        }
        args[1] = receiver_type
        ++arg_count
        ++method_count
        method_names[method_count] = name
        method_returns[method_count] = return_type
        method_arg_count[method_count] = arg_count
        for (i = 1; i <= arg_count; ++i) {
            method_args[method_count, i] = args[i]
        }
        clear_args()
    } else {
        emit_function(return_type, name)
        clear_args()
    }
}

function parse_callback_decl(decl,    open_name, close_name, name,
                             return_type, rest, open_pos, close_pos,
                             arguments) {
    sub(/[ \t]*;[ \t]*$/, "", decl)
    sub(/^typedef[ \t]+/, "", decl)
    if (!match(decl, /\(\*[A-Za-z_][A-Za-z0-9_]*\)/)) {
        report_error("invalid callback typedef: " decl)
        return
    }
    open_name = RSTART
    close_name = RSTART + RLENGTH - 1
    return_type = trim(substr(decl, 1, open_name - 1))
    name = substr(decl, open_name + 2, close_name - open_name - 2)
    rest = substr(decl, close_name + 1)
    open_pos = index(rest, "(")
    close_pos = last_index(rest, ")")
    if (open_pos == 0 || close_pos < open_pos) {
        report_error("invalid callback arguments: " decl)
        return
    }
    arguments = substr(rest, open_pos + 1, close_pos - open_pos - 1)
    return_type = resolve_known_types(return_type)
    parse_arguments(arguments)
    emit_callback(return_type, name)
    clear_args()
}

function parse_simple_typedef(line,    text, alias, alias_c, source, i) {
    text = line
    sub(/^typedef[ \t]+/, "", text)
    sub(/[ \t]*;[ \t]*$/, "", text)
    if (!match(text, /[A-Za-z_][A-Za-z0-9_]*$/)) {
        report_error("invalid typedef: " line)
        return
    }
    alias = substr(text, RSTART, RLENGTH)
    source = trim(substr(text, 1, RSTART - 1))
    alias_c = c_type_alias(alias)
    source = resolve_known_types(source)
    if (pending_idl_attr_count != 0) {
        clear_idl_contract()
        for (i = 1; i <= pending_idl_attr_count; ++i) {
            append_idl_contract(pending_idl_attrs[i])
        }
        append_idl_contract(line)
        emit_idl_contract()
        clear_pending_idl_attrs()
    }
    if (pending_native != 0) {
        known_type[alias] = source
        return
    }
    print "typedef " source " " alias_c ";"
    print ""
    known_type[alias] = alias_c
}

function finish_decl() {
    decl_text = trim(decl_text)
    if (decl_kind == "method") {
        parse_callable_decl(decl_text, "method", decl_const)
    } else if (decl_kind == "function") {
        parse_callable_decl(decl_text, "function", 0)
    } else if (decl_kind == "callback") {
        parse_callback_decl(decl_text)
    }
    mode = decl_after_mode
    decl_kind = ""
    decl_text = ""
    decl_const = 0
    decl_after_mode = ""
    clear_pending_attrs()
    clear_pending_idl_attrs()
}

mode == "comment" {
    print $0
    if ($0 ~ /\*\//) {
        mode = comment_return_mode
        comment_return_mode = ""
    }
    next
}

mode == "raw" {
    if ($0 == ">>") {
        print ""
        mode = ""
    } else {
        print $0
    }
    next
}

mode == "record" {
    line = trim($0)
    if (line == "};") {
        finish_record()
        mode = ""
    } else if (line != "" && line !~ /^\/\//) {
        emit_struct_field(line)
    }
    next
}

mode == "enum" {
    line = trim($0)
    if (line == "};") {
        finish_enum()
        mode = ""
    } else if (line != "" && line !~ /^\/\//) {
        sub(/,$/, "", line)
        enum_entries[++enum_count] = line
    }
    next
}

mode == "decl" {
    if (decl_kind == "method") {
        append_idl_contract($0)
    }
    decl_text = decl_text " " trim($0)
    if (decl_text ~ /\)[ \t]*(const[ \t]*)?;$/) {
        finish_decl()
    }
    next
}

mode == "interface" {
    line = trim($0)
    if (line == "};") {
        append_idl_contract("};")
        emit_interface()
        mode = ""
        next
    }
    if (line == "" || line ~ /^\/\//) {
        next
    }
    if (line ~ /^\/\*/) {
        print $0
        if ($0 !~ /\*\//) {
            mode = "comment"
            comment_return_mode = "interface"
        }
        next
    }
    append_idl_contract($0)
    line = take_attrs(line)
    if (line == "") {
        next
    }
    begin_decl("method", line, "interface", pending_const)
    next
}

mode == "coclass" {
    line = trim($0)
    if (line == "};") {
        append_idl_contract("};")
        emit_coclass_contract()
        mode = ""
        next
    }
    if (line == "" || line ~ /^\/\//) {
        next
    }
    if (line ~ /^\/\*/) {
        print $0
        if ($0 !~ /\*\//) {
            mode = "comment"
            comment_return_mode = "coclass"
        }
        next
    }
    append_idl_contract($0)
    line = take_attrs(line)
    if (line == "") {
        next
    }
    if (line !~ /^interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*;$/) {
        report_error("invalid coclass member: " line)
    }
    clear_pending_attrs()
    clear_pending_idl_attrs()
    next
}

{
    line = trim($0)
    if (line == "" || line ~ /^\/\// || line ~ /^#/) {
        next
    }
    if (line ~ /^\/\*/) {
        print $0
        if ($0 !~ /\*\//) {
            mode = "comment"
            comment_return_mode = ""
        }
        next
    }
    line = take_attrs(line)
    if (line == "") {
        next
    }
    if (line == "cpp_quote <<") {
        mode = "raw"
        clear_pending_attrs()
        next
    }
    if (line ~ /^module[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        clear_pending_attrs()
        next
    }
    if (line == "};") {
        clear_pending_attrs()
        next
    }
    if (line ~ /^struct[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*;$/) {
        sub(/^struct[ \t]+/, "", line)
        sub(/[ \t]*;$/, "", line)
        printf("struct %s;\n", c_struct_tag(line))
        clear_pending_attrs()
        next
    }
    if (line ~ /^interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*;$/) {
        sub(/^interface[ \t]+/, "", line)
        sub(/[ \t]*;$/, "", line)
        emit_interface_forward(line)
        next
    }
    if (line ~ /^struct[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        sub(/^struct[ \t]+/, "", line)
        sub(/[ \t]*\{$/, "", line)
        register_struct_type(line)
        printf("typedef struct %s {\n", record_tag)
        mode = "record"
        clear_pending_attrs()
        next
    }
    if (line ~ /^enum[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        sub(/^enum[ \t]+/, "", line)
        sub(/[ \t]*\{$/, "", line)
        register_enum_type(line)
        printf("typedef enum %s {\n", enum_tag)
        clear_enum()
        mode = "enum"
        clear_pending_attrs()
        next
    }
    if (line ~ /^interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        sub(/^interface[ \t]+/, "", line)
        sub(/[ \t]*\{$/, "", line)
        setup_interface(line)
        clear_methods()
        clear_idl_contract()
        for (i = 1; i <= pending_idl_attr_count; ++i) {
            append_idl_contract(pending_idl_attrs[i])
        }
        append_idl_contract("interface " interface_name " {")
        clear_pending_idl_attrs()
        mode = "interface"
        next
    }
    if (line ~ /^coclass[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        if (pending_classid == "") {
            report_error("coclass is missing classid attribute: " line)
        }
        clear_idl_contract()
        for (i = 1; i <= pending_idl_attr_count; ++i) {
            append_idl_contract(pending_idl_attrs[i])
        }
        append_idl_contract(line)
        clear_pending_idl_attrs()
        mode = "coclass"
        next
    }
    if (line ~ /^typedef[ \t]+/ && line ~ /\(\*/) {
        begin_decl("callback", line, "", 0)
        next
    }
    if (line ~ /^typedef[ \t]+/) {
        parse_simple_typedef(line)
        clear_pending_attrs()
        next
    }
    if (line ~ /\(/) {
        begin_decl("function", line, "", 0)
        next
    }
    report_error("unexpected line: " line)
    clear_pending_attrs()
}

END {
    if (mode != "") {
        printf("%s:%d: unterminated %s block\n", FILENAME, FNR, mode) > \
            "/dev/stderr"
        has_error = 1
    }
    if (has_error != 0) {
        exit 1
    }
    emit_footer()
}
