# A small source lexer: preserve shell spelling, skip inert heredoc payloads,
# and join continued statements without executing expansions or shell code.
function expansion_end(text, start,    i, ch, quote, nested, limit) {
    quote = ""; limit = length(text)
    for (i = start + 2; i <= limit; i++) {
        ch = substr(text, i, 1)
        if (ch == "\\" && quote != "\047") { i++; continue }
        if (quote == "\047") {
            if (ch == quote) quote = ""
        } else if (substr(text, i, 2) == "${") {
            nested = expansion_end(text, i)
            if (!nested) return 0
            i = nested - 1
        } else if (ch == "\"" || ch == "\047") {
            if (quote == "") quote = ch
            else if (quote == ch) quote = ""
        } else if (ch == "}" && quote == "") return i + 1
    }
    return 0
}
function shell_words(text, tokens, starts, ends,    count, i, start, ch, quote, nested, limit) {
    i = 1; limit = length(text)
    while (i <= limit) {
        ch = substr(text, i, 1)
        if (ch ~ /[[:space:]]/) { i++; continue }
        if (ch == "#") break
        start = i
        if (ch ~ /[;|&()<>]/) {
            i++
            if (substr(text, i, 1) == ch) i++
        } else {
            quote = ""
            while (i <= limit) {
                ch = substr(text, i, 1)
                if (ch == "\\" && quote != "\047") { i += 2; continue }
                if (quote == "\047") {
                    if (ch == quote) quote = ""
                } else if (substr(text, i, 2) == "${") {
                    nested = expansion_end(text, i)
                    if (!nested) return -1
                    i = nested; continue
                } else if (ch == "\"" || ch == "\047") {
                    if (quote == "") quote = ch
                    else if (quote == ch) quote = ""
                } else if (quote == "" && ch ~ /[[:space:];|&()<>]/) break
                i++
            }
            if (quote != "") return -1
        }
        tokens[++count] = substr(text, start, i - start)
        starts[count] = start; ends[count] = i
    }
    return count
}
function unquote(word,    i, ch, quote, result) {
    for (i = 1; i <= length(word); i++) {
        ch = substr(word, i, 1)
        if (ch == "\\" && quote != "\047") {
            result = result substr(word, ++i, 1)
        } else if (ch == "\"" || ch == "\047") {
            if (quote == "") quote = ch
            else if (quote == ch) quote = ""
            else result = result ch
        } else result = result ch
    }
    return result
}
function reset_shell() {
    pending = ""; heredoc_count = heredoc_next = 0
}
function shell_line(line, number,    logical, n, i, word, candidate, tokens, starts, ends) {
    if (heredoc_next < heredoc_count) {
        candidate = line
        sub(/\r?\n$/, "", candidate)
        if (heredoc_tabs[heredoc_next + 1]) sub(/^\t+/, "", candidate)
        if (candidate == heredoc[heredoc_next + 1]) heredoc_next++
        return
    }
    if (pending == "") statement_line = number
    pending = pending line
    if (line ~ /\\\r?\n$/) return
    logical = pending; gsub(/\\\r?\n/, " ", logical)
    n = shell_words(logical, tokens, starts, ends)
    if (n < 0) return
    for (i = 1; i < n; i++) if (tokens[i] == "<<") {
        word = tokens[i + 1]
        heredoc_tabs[++heredoc_count] = substr(word, 1, 1) == "-"
        if (heredoc_tabs[heredoc_count]) word = substr(word, 2)
        if (word == "" && i + 2 <= n) word = tokens[i + 2]
        heredoc[heredoc_count] = unquote(word)
    }
    inspect_tokens(logical, pending, statement_line, tokens, starts, ends, n)
    pending = ""
}
