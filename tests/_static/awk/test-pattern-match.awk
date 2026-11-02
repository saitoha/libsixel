# Inspect literal shell operands, never execute a test or its expansions.
function parameter(word, out,    name, rest) {
    out[1] = out[2] = out[3] = ""
    if (substr(word, 1, 1) == "\"" && substr(word, length(word)) == "\"")
        word = substr(word, 2, length(word) - 2)
    if (word ~ /^\$[A-Za-z_][A-Za-z0-9_]*$/) {
        out[1] = substr(word, 2); return 1
    }
    if (word !~ /^\$\{[A-Za-z_][A-Za-z0-9_]*/ || expansion_end(word, 1) != length(word) + 1)
        return 0
    name = substr(word, 3); sub(/[^A-Za-z0-9_].*$/, "", name)
    rest = substr(word, length(name) + 3, length(word) - length(name) - 3)
    out[1] = name
    if (rest == "") return 1
    if (rest !~ /^[#%]/) { out[1] = ""; return 0 }
    out[2] = substr(rest, 1, 1)
    if (substr(rest, 2, 1) == out[2]) out[2] = out[2] out[2]
    out[3] = substr(rest, length(out[2]) + 1)
    return 1
}
function original(name) { return (name in aliases) ? aliases[name] : name }
function pattern_syntax(word, final_star,    i, ch, quote, last) {
    for (i = 1; i <= length(word); i++) {
        ch = substr(word, i, 1)
        if (ch == "\\" && quote != "\047") { i++; continue }
        if (quote != "") { if (ch == quote) quote = "" }
        else if (ch == "\"" || ch == "\047") quote = ch
        else if (index("*?[$", ch)) {
            if (!final_star) return 1
            if (ch == "*") last = i
        }
    }
    return final_star && last == length(word)
}
function removed(a, b,    name) {
    if (a[1] == "" || b[1] == "" || b[2] != "") return 0
    name = a[2] != "" ? original(a[1]) : derived[a[1]]
    return name != "" && name == original(b[1])
}
function reset_pattern(    key) {
    reset_shell()
    for (key in aliases) delete aliases[key]
    for (key in derived) delete derived[key]
}
function inspect_tokens(logical, raw, line, tokens, starts, ends, n,
                        i, value, name, parsed, left, right, operand, operator, found) {
    for (i = 1; i <= n; i++) {
        value = tokens[i]
        if (value ~ /^[A-Za-z_][A-Za-z0-9_]*=/) {
            name = value; sub(/=.*/, "", name)
            sub(/^[^=]*=/, "", value)
            delete aliases[name]; delete derived[name]
            if (parameter(value, parsed) && parsed[1] != name) {
                if (parsed[2] != "") derived[name] = original(parsed[1])
                else if (parsed[1] in derived) derived[name] = derived[parsed[1]]
                else aliases[name] = original(parsed[1])
            }
        }
        if (parameter(value, parsed) && parsed[2] == "#" &&
            length(parsed[3]) >= 3 && substr(parsed[3], 1, 1) == "*" &&
            pattern_syntax(parsed[3], 1)) found = 1
        if (tokens[i] != "test" && tokens[i] != "[" && tokens[i] != "[[") continue
        if (i > 1 && tokens[i - 1] !~ /^(;|&&|\|\||!|\{|\(|\)|if|elif|else|then|do|while|until|command|builtin)$/) continue
        operand = i + 1
        if (tokens[operand] == "!") operand++
        if (operand + 2 > n) continue
        parameter(tokens[operand], left); parameter(tokens[operand + 2], right)
        operator = tokens[operand + 1]
        if (operator ~ /^(=|==|!=)$/ && (removed(left, right) || removed(right, left))) found = 1
        if (tokens[i] == "[[" && (operator == "=~" ||
            (operator ~ /^(=|==|!=)$/ && pattern_syntax(tokens[operand + 2], 0)))) found = 1
    }
    if (found) {
        if (control) { control_count++; return }
        sub(/[\r\n]+$/, "", raw)
        name = substr(current_path, length(root) + 2)
        value = sha256(raw)
        finding[name SUBSEP line SUBSEP value] = 1
        finding_count++
    }
}
BEGIN {
    pattern_controls()
    reset_pattern()
}
{
    current_path = $0; source_line = candidate = 0
    reset_pattern()
    while ((read_status = (getline source_text < current_path)) > 0) {
        source_lines[++source_line] = source_text
        if (source_text ~ /\$\{[A-Za-z_][A-Za-z0-9_]*[#%]|\[\[/)
            candidate = 1
    }
    close(current_path)
    # Without a removal or a Bash predicate, no alias chain can introduce
    # a finding. Tokenize candidate files completely, including heredocs.
    if (candidate) for (source_index = 1; source_index <= source_line; source_index++)
        shell_line(source_lines[source_index] "\n", source_index)
    if (read_status < 0 || pending != "") {
        print "# unreadable or unterminated shell source: " current_path
        failed = 1
    }
}

END {
    if (NR == 0) { print "# no shell TAP sources found"; failed = 1 }
    # Exceptions are exact source identities, not broad filename waivers.
    validate_whitelist(root "/tests/_static/data/test-pattern-match-whitelist.json")
    for (key in finding) if (!(key in approved)) {
        split(key, fields, SUBSEP)
        print "# " fields[1] ":" fields[2] ": use case for pattern matching (sha256 " fields[3] ")"
        failed = 1
    }
    exit failed
}
