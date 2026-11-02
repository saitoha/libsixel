# Inspect C tokens without a compiler or an optional language runtime.
function strip_line(line,    result, token, pattern) {
    # Preserve include operands separately from identifier-bearing C tokens.
    # Only copy one line at a time, even for very large translation units.
    pattern = "\"([^\"\\\\]|\\\\.)*\"|\047([^\047\\\\]|\\\\.)*\047|/\\*|//"
    result = include_line = ""
    while (line != "") {
        if (lexer_block) {
            if (!match(line, /\*\//)) break
            line = substr(line, RSTART + RLENGTH)
            lexer_block = 0
        } else if (match(line, pattern)) {
            result = result substr(line, 1, RSTART - 1)
            include_line = include_line substr(line, 1, RSTART - 1)
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == "/*") {
                lexer_block = 1; result = result " "; include_line = include_line " "
            } else if (token == "//") break
            else { result = result " "; include_line = include_line token }
        } else {
            result = result line; include_line = include_line line; break
        }
    }
    return result
}
function integer_header(line) {
    return line ~ /^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](stdint|inttypes|sixel)\.h[>"]/
}
function quoted_header(line) {
    if (line !~ /^[[:space:]]*#[[:space:]]*include[[:space:]]*"/) return ""
    sub(/^[^"]*"/, "", line); sub(/".*/, "", line)
    return line
}
function provider(path,    line, status, headers, count, i, name, dir, saved_block, result) {
    if (path in visiting) return 0
    visiting[path] = 1
    saved_block = lexer_block; lexer_block = 0
    while ((status = (getline line < path)) > 0) {
        strip_line(line)
        if (integer_header(include_line)) { result = 1; break }
        name = quoted_header(include_line)
        if (name != "") headers[++count] = name
    }
    close(path); lexer_block = saved_block
    if (status < 0) return 0
    if (result) return 1
    dir = path; sub(/[^\/]*$/, "", dir)
    for (i = 1; i <= count; i++) {
        name = headers[i]
        if (provider(dir name) || provider(root "/" name) ||
            provider(root "/src/" name)) return 1
    }
    return 0
}
function inspect(    code, i, words, j, count, name, ready, dir, errors, definition) {
    if (!candidate) return 0
    lexer_block = 0
    dir = file; sub(/[^\/]*$/, "", dir)
    for (i = 1; i <= line_count; i++) {
        code = strip_line(lines[i])
        # Most lines contain none of the identifiers under inspection.
        if (code ~ /(^|[^[:alnum:]_])(near|far|interface)([^[:alnum:]_]|$)/) {
            count = split(code, words, /[^[:alnum:]_]+/)
            for (j = 1; j <= count; j++) {
                if (words[j] !~ /^(near|far|interface)$/) continue
                if (!control) print file ":" i ": identifier collides with Windows macro: " words[j]
                errors++
            }
        }
        if (integer_header(include_line)) ready = 1
        name = quoted_header(include_line)
        definition = code ~ /^[[:space:]]*#[[:space:]]*define[[:space:]]+SIZE_MAX([^[:alnum:]_]|$)/
        if (fallback && !ready && name != "") {
            for (j in visiting) delete visiting[j]
            if (provider(dir name) || provider(root "/" name) ||
                provider(root "/src/" name)) ready = 1
        }
        if (definition && !ready) {
            if (!control) print file ":" i ": SIZE_MAX fallback precedes integer header"
            errors++
        }
    }
    return errors
}
function specimen(text) {
    line_count = split(text, lines, "\n"); candidate = fallback = 1
    return inspect()
}
BEGIN {
    control = 1; file = "control"
    if (specimen("double nearest_distance, farthest_distance;") ||
        specimen("/* near */ char *s = \"far\"; // interface\n") ||
        specimen("#include <stdint.h>\n#define SIZE_MAX 1") ||
        !specimen("int interface;") || !specimen("double near, far;") ||
        !specimen("#define SIZE_MAX 1\n#include <stdint.h>") ||
        !specimen("/* #include <stdint.h> */\n#define SIZE_MAX 1")) {
        print "preprocessor portability controls failed"
        failed = 1; exit 1
    }
    control = candidate = fallback = line_count = 0
}
FNR == 1 {
    if (inspect()) failed = 1
    line_count = candidate = fallback = 0; file = FILENAME
}
{
    lines[++line_count] = $0
    if ($0 ~ /(^|[^[:alnum:]_])(near|far|interface)([^[:alnum:]_]|$)/) candidate = 1
    if ($0 ~ /#[[:space:]]*define[[:space:]]+SIZE_MAX/) candidate = fallback = 1
}
END { if (inspect()) failed = 1; exit failed }
