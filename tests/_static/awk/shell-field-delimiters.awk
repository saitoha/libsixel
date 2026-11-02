# Literal pipes in parameter-removal patterns are alternation in some ksh
# versions. Ignore comments and single-quoted shell data, not double quotes.
function inspect(    i, ch, single, double_quote, line) {
    # Almost all files have no candidate. Tokenize only possible violations.
    if (!candidate) return
    source = ""
    for (i = 1; i <= line_count; i++) source = source lines[i] "\n"
    line = 1
    for (i = 1; i <= length(source); i++) {
        ch = substr(source, i, 1)
        if (ch == "\n") line++
        if (single) {
            if (ch == "\047") single = 0
            continue
        }
        if (ch == "\\") {
            if (substr(source, i + 1, 1) == "\n") line++
            i++; continue
        }
        if (ch == "\047" && !double_quote) { single = 1; continue }
        if (ch == "\"") { double_quote = !double_quote; continue }
        if (ch == "#" && !double_quote &&
            (i == 1 || substr(source, i - 1, 1) ~ /[[:space:];]/)) {
            while (i <= length(source) && substr(source, i, 1) != "\n") i++
            line++; continue
        }
        if (ch == "$" && substr(source, i) ~ /^\$\{[A-Za-z_][A-Za-z_0-9]*[%#][%#]?\|/) {
            print file ":" line ": escape literal | in removal pattern"
            failed = 1
        }
    }
}
FNR == 1 { inspect(); line_count = candidate = 0; file = FILENAME }
{
    lines[++line_count] = $0
    if ($0 ~ /\$\{[A-Za-z_][A-Za-z_0-9]*[%#][%#]?\|/) candidate = 1
}
END { inspect(); exit failed }
