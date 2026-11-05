# Reject raw string copying calls that trigger MSVC CRT diagnostics.
function strip(line,    result, token) {
    while (line != "") {
        if (comment) {
            if (!match(line, /\*\//)) break
            line = substr(line, RSTART + RLENGTH)
            comment = 0
        } else if (match(line, /"([^"\\]|\\.)*"|'([^'\\]|\\.)*'|\/\*|\/\//)) {
            result = result substr(line, 1, RSTART - 1)
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == "//") break
            if (token == "/*") comment = 1
            result = result " "
        } else {
            result = result line
            break
        }
    }
    return result
}
function inspect(text,    n, input, i, code) {
    comment = 0
    gsub(/\\\n/, "", text)
    # Most files have no raw identifier; avoid lexing their comments/literals.
    if (text !~ /(^|[^[:alnum:]_])(strcpy|strcat)([^[:alnum:]_]|$)/) return 0
    n = split(text, input, "\n")
    for (i = 1; i <= n; ++i) code = code "\n" strip(input[i])
    return code ~ /(^|[^[:alnum:]_])(strcpy|strcat)[[:space:]]*\(/
}
BEGIN {
    if (!inspect("strcpy(dst, src);") ||
        !inspect("strcat\n /* wrapped line */ (dst, src);") ||
        !inspect("str\\\ncpy(dst, src);") ||
        inspect("sixel_compat_strcpy(dst, size, src);") ||
        inspect("memcpy(dst, src, size);") ||
        inspect("/* strcpy( */\nputs(\"strcat(\"); // strcpy(")) {
        print "CLI string copy controls failed"
        failed = 1
        exit 1
    }
}
# Only candidate files need a second lexical pass. Join continued lines in
# this cheap scan too, so split identifiers cannot bypass candidate selection.
FNR == 1 { continued = "" }
{
    logical = continued $0
    if (sub(/\\$/, "", logical)) {
        continued = logical
        next
    }
    continued = ""
    if (logical ~ /(^|[^[:alnum:]_])(strcpy|strcat)([^[:alnum:]_]|$)/)
        candidates[FILENAME] = 1
}
END {
    for (file in candidates) {
        text = ""
        while ((status = getline line < file) > 0) text = text line "\n"
        close(file)
        if (status < 0) {
            print file ": cannot read source"
            failed = 1
        } else if (inspect(text)) {
            print file ": use a bounded CRT adapter or length-checked byte copy"
            failed = 1
        }
    }
    exit failed
}
