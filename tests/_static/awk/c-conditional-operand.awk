# Reject adjacent question/colon tokens outside comments and literals.
function code(text) {
    if ((question && text ~ /^[[:space:]]*:/) ||
        text ~ /\?[[:space:]]*:/) return 1
    if (text ~ /[^[:space:]]/) question = (text ~ /\?[[:space:]]*$/)
    return 0
}
function inspect(line,    token, prefix) {
    # Most numeric tables and ordinary statements cannot contain this syntax.
    if (!comment && !question && line !~ /\?|\/\*/) return 0
    while (line != "") {
        if (comment) {
            if (!match(line, /\*\//)) return 0
            line = substr(line, RSTART + RLENGTH)
            comment = 0
        } else if (match(line, /"([^"\\]|\\.)*"|'([^'\\]|\\.)*'|\/\*|\/\//)) {
            token = substr(line, RSTART, RLENGTH)
            prefix = substr(line, 1, RSTART - 1)
            line = substr(line, RSTART + RLENGTH)
            if (code(prefix)) return 1
            if (token == "//") return 0
            if (token == "/*") comment = 1
            else question = 0
        } else return code(line)
    }
    return 0
}
function consume(line) {
    # Skip ordinary physical lines before concatenation or lexical work.
    if (!comment && !question && pending == "" &&
        !index(line, "?") && !index(line, "/*") && !index(line, "\\")) return 0
    # Splice physical lines before recognizing comments and literal tokens.
    line = pending line
    if (sub(/\\$/, "", line)) {
        pending = line
        return 0
    }
    pending = ""
    return inspect(line)
}
function sample(text,    lines, count, i, result) {
    comment = question = 0
    pending = ""
    count = split(text, lines, "\n")
    result = 0
    for (i = 1; i <= count; ++i) result += consume(lines[i])
    return result != 0
}
BEGIN {
    if (!sample("value ?: fallback") ||
        !sample("value ? /* note */ : fallback") ||
        !sample("value ?\n: fallback") ||
        !sample("value ? \\\n: fallback") ||
        !sample("value ? // note\n: fallback") ||
        !sample("value ? /* note\ncontinued */ : fallback") ||
        sample("value ? value : fallback") ||
        sample("value ? \"?:\" : fallback") ||
        sample("value ? '?' : ':'") ||
        sample("value ? \"\\\" ?: \" : fallback") ||
        sample("/* ?: */ value") || sample("// ?: \nvalue") ||
        sample("value ? \"a\" /* note */ : fallback")) {
        print "conditional operand scanner controls failed"
        failed = 1
        exit 1
    }
}
FNR == 1 { comment = question = 0; pending = "" }
{
    if (consume($0)) {
        print FILENAME ":" FNR ": use an explicit middle operand instead of GNU ?:"
        failed = 1
    }
}
END { if (failed) exit 1 }
