# Assessment file opens preserve cross-runtime path conversion.
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
    n = split(text, input, "\n")
    for (i = 1; i <= n; ++i) code = code "\n" strip(input[i])
    return code ~ /(^|[^[:alnum:]_])fopen[[:space:]]*\(/
}
BEGIN {
    if (!inspect("fopen(\"NAME\");") ||
        !inspect("fopen\n /* wrapped line */ (name);") ||
        inspect("sixel_compat_fopen(name);") ||
        inspect("/* fopen( */\nputs(\"fopen(\"); // fopen(")) {
        print "assessment file path controls failed"
        failed = 1
        exit 1
    }
}
# Keep the common path to a literal search. Only candidate files need a
# second read for comment/literal removal and continued call expressions.
/(^|[^[:alnum:]_])fopen([^[:alnum:]_]|$)/ {
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
            print file ": use sixel_compat_fopen for cross-runtime paths"
            failed = 1
        }
    }
    exit failed
}
