# Match z-length conversions in C string literals, including adjacent tokens.
function has_size_format(text) {
    gsub(/%%/, "", text)
    return text ~ /%[0-9.$+ #*'-]*z[diouxXn]/
}

function scan(line,    token, prefix, found) {
    while (line != "") {
        if (comment) {
            if (!match(line, /\*\//)) break
            line = substr(line, RSTART + RLENGTH)
            comment = 0
        } else if (match(line, /"([^"\\]|\\.)*"|'([^'\\]|\\.)*'|\/\*|\/\//)) {
            prefix = substr(line, 1, RSTART - 1)
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (prefix ~ /[^ \t\r]/) literal = ""
            if (token == "//") break
            if (token == "/*") comment = 1
            else if (substr(token, 1, 1) == "\"") {
                literal = literal substr(token, 2, length(token) - 2)
                if (has_size_format(literal)) {
                    found = 1
                    literal = ""
                }
            } else literal = ""
        } else {
            if (line ~ /[^ \t\r]/) literal = ""
            break
        }
    }
    return found
}

BEGIN {
    if (!scan("\"pixels=%zu\"") || !scan("\"%08zx\"") ||
        !scan("\"%\" /* comment */ \"zu\"") ||
        scan("// \"%zu\"") || scan("/* \"%zu\" */") ||
        scan("\"%%zu\"") || scan("\"%\" SIXEL_PRI_SIZE")) {
        print "size format scanner controls failed"
        exit 1
    }
}

FNR == 1 { comment = 0; literal = ""; pending = "" }
{
    # Backslash-newline is removed before C recognizes strings and comments.
    line = pending $0
    if (sub(/\\$/, "", line)) {
        pending = line
        next
    }
    pending = ""
    if (scan(line)) {
        print FILENAME ":" FNR ": use SIXEL_PRI_SIZE and SIXEL_PRINTF_SIZE"
        failed = 1
    }
}
END { if (failed) exit 1 }
