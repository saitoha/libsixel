# Inspect literal tokens only: comments and escaped backslashes are not escapes.
# Adjacent string literals preserve bytes while separating the octal boundary.
function literal(token,    i, digit, count) {
    while (match(token, /\\./)) {
        i = RSTART + 1
        digit = substr(token, i, 1)
        if (digit ~ /^[0-7]$/) {
            count = 0
            while (count < 3 && substr(token, i, 1) ~ /^[0-7]$/) {
                ++i
                ++count
            }
            if (substr(token, i, 1) ~ /^[0-9]$/) return 1
        } else ++i
        token = substr(token, i)
    }
    return 0
}
function inspect(line,    token, errors) {
    # Most source lines have neither a comment opener nor a candidate escape.
    # Avoid tokenizing large generated numeric tables and ordinary strings.
    if (!comment && line !~ /\/\*|\\[0-7]+[0-9]/) return 0
    while (line != "") {
        if (comment) {
            if (!match(line, /\*\//)) break
            line = substr(line, RSTART + RLENGTH)
            comment = 0
        } else if (match(line, /"([^"\\]|\\.)*"|'([^'\\]|\\.)*'|\/\*|\/\//)) {
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == "//") break
            if (token == "/*") comment = 1
            else errors += literal(token)
        } else break
    }
    return errors
}
BEGIN {
    # Controls cover all escape widths, decimal digits, escaped slashes,
    # adjacent literals, character constants, comments and a fourth octal digit.
    for (digit = 0; digit <= 9; ++digit) {
        if (!inspect("\"\\033" digit "\"") ||
            inspect("\"\\033\" \"" digit "\"")) {
            print "octal escape digit controls failed"
            exit 1
        }
    }
    if (!inspect("\"\\08\"") || !inspect("\"\\079\"") ||
        !inspect("\"\\0338\"") || !inspect("\"\\0339\"") ||
        !inspect("'\\078'") || inspect("\"\\033\" \"8\"") ||
        !inspect("\"\\0337\"") || !inspect("\"\\03308\"") ||
        inspect("\"\\\\0338\"") || inspect("// \"\\0338\"") ||
        inspect("/* \"\\0338\" */") ||
        !inspect("\"\\\"\\0338\"")) {
        print "octal escape scanner controls failed"
        exit 1
    }
}
FNR == 1 { comment = 0; pending = "" }
{
    # C splices backslash-newline before recognizing comments and literals.
    line = pending $0
    if (sub(/\\$/, "", line)) {
        pending = line
        next
    }
    pending = ""
    if (inspect(line)) {
        print FILENAME ":" FNR ": split the literal after an octal escape before a decimal digit (MSVC C4125)"
        failed = 1
    }
}
END { if (failed) exit 1 }
