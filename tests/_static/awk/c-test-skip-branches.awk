# Detect a compile-time skip followed by an unconditional test body.
# This deliberately recognizes standalone return-77 branches, not general
# control flow. Runtime conditions and branches containing other work are
# left to the compiler; no target macros are evaluated here.
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
function inspect(    i, code, level, braces, pending, errors, scratch, previous) {
    comment = 0
    for (i = 1; i <= count; ++i) {
        code = strip(lines[i])
        if (code ~ /^[[:space:]]*$/) continue
        if (code ~ /^[[:space:]]*#/) {
            # Only diagnose immediately following C, not a separately
            # guarded body whose condition would require preprocessing.
            pending = 0
            if (code ~ /^[[:space:]]*#[[:space:]]*if(n?def)?([[:space:](]|$)/) {
                ++level
                start[level] = braces
                # A runtime if/loop outside the preprocessor branch can
                # still control its return. Require a completed statement
                # or the function's opening brace before this directive.
                simple[level] = braces == 1 &&
                    previous ~ /[;{][[:space:]]*$/
                skipped[level] = 0
            } else if (code ~ /^[[:space:]]*#[[:space:]]*(else|elif)([[:space:]]|$)/) {
                simple[level] = 0
                braces = start[level]
            } else if (code ~ /^[[:space:]]*#[[:space:]]*endif([[:space:]]|$)/) {
                if (level > 0) {
                    if (simple[level]) pending = skipped[level]
                    --level
                }
            } else if (level > 0) simple[level] = 0
            continue
        }
        if (pending) {
            if (code !~ /^[[:space:]]*}/) {
                if (!control) print file ":" pending ": compile-time skip leaves an unreachable test body; use an alternative branch"
                ++errors
            }
            pending = 0
        }
        if (level > 0 && simple[level]) {
            if (code ~ /^[[:space:]]*return[[:space:]]+77[[:space:]]*;[[:space:]]*$/)
                skipped[level] = i
            else if (code !~ /^[[:space:]]*\(void\)[[:space:]]*[[:alnum:]_]+[[:space:]]*;[[:space:]]*$/)
                simple[level] = 0
        }
        scratch = code
        braces += gsub(/{/, "", scratch) - gsub(/}/, "", scratch)
        previous = code
    }
    return errors
}
function specimen(text) {
    count = split(text, lines, "\n")
    return inspect()
}
BEGIN {
    control = 1
    prefix = "int test(void)\n{\n#if DISABLED\n"
    if (!specimen(prefix "return 77;\n#endif\nrun();\n}") ||
        !specimen(prefix "/* skip */\n(void)argv;\nreturn 77;\n#endif\nrun();\n}") ||
        specimen(prefix "return 77;\n#else\nrun();\n#endif\n}") ||
        specimen(prefix "if (absent)\nreturn 77;\n#endif\nrun();\n}") ||
        specimen("int test(void)\n{\nif (absent)\n#if FEATURE\nreturn 77;\n#endif\nrun();\n}") ||
        specimen(prefix "return 77;\n#endif\n}") ||
        specimen(prefix "/* return 77; */\n#endif\nrun();\n}") ||
        specimen(prefix "puts(\"return 77;\");\n#endif\nrun();\n}")) {
        print "compile-time skip controls failed"
        failed = 1
        exit 1
    }
    control = count = candidate = 0
}
FNR == 1 {
    if (candidate && inspect()) failed = 1
    count = candidate = 0
    file = FILENAME
}
{
    lines[++count] = $0
    if ($0 ~ /return[[:space:]]+77[[:space:]]*;/) candidate = 1
}
END {
    if (!control && candidate && inspect()) failed = 1
    exit failed
}
