# C runner tests use the shared CRT wrapper for numeric scanning.
# Standalone extension probes do not link the libsixel compatibility layer.
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
    return code ~ /(^|[^[:alnum:]_])(v?sscanf)[[:space:]]*\(/
}
function finish(    text, i) {
    if (!candidate) return
    for (i = 1; i <= count; ++i) text = text lines[i] "\n"
    if (inspect(text)) {
        print file ": use sixel_compat_sscanf or sixel_compat_vsscanf"
        failed = 1
    }
}
BEGIN {
    if (!inspect("sscanf(input, format, &n);") ||
        !inspect("vsscanf\n /* wrapped line */ (input, format, args);") ||
        inspect("sixel_compat_sscanf(input, format, &n);") ||
        inspect("/* sscanf( */\nputs(\"sscanf(\"); // vsscanf(")) {
        print "CRT scanning controls failed"
        failed = 1
        exit 1
    }
}
FNR == 1 {
    finish()
    count = candidate = 0
    file = FILENAME
}
{
    lines[++count] = $0
    if ($0 ~ /sscanf/) candidate = 1
}
END { finish(); exit failed }
