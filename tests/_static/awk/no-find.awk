# Inspect command positions, including literal nested shell programs.
function contains_find(source,    tokens, starts, ends, n, i, raw, text, command, shell, shell_code, nested, fragment) {
    gsub(/\$\(shell[[:space:]]+/, "$(", source)
    n = shell_words(source, tokens, starts, ends)
    command = 1
    for (i = 1; i <= n; i++) {
        raw = tokens[i]
        if (substr(raw, 1, 1) != "\047") {
            nested = raw
            while (match(nested, /\$\([^()]*\)|`[^`]*`/)) {
                fragment = substr(nested, RSTART, RLENGTH)
                nested = substr(nested, RSTART + RLENGTH)
                if (substr(fragment, 1, 1) == "`") fragment = substr(fragment, 2, length(fragment) - 2)
                else fragment = substr(fragment, 3, length(fragment) - 3)
                sub(/^shell[[:space:]]+/, "", fragment)
                if (contains_find(fragment)) return 1
            }
        }
        text = unquote(raw)
        if (shell_code) {
            if (contains_find(text)) return 1
            shell_code = 0
        }
        if (shell && text == "-c") shell_code = 1
        if (text ~ /^(;|;;|\||\|\||&|&&|\(|\)|\{|if|elif|then|else|do|while|until|!)$/) {
            command = 1; shell = 0; continue
        }
        if (!command) continue
        sub(/^[@+-]+/, "", text)
        if (text == "" || text ~ /^[A-Za-z_][A-Za-z0-9_]*=/) continue
        if (text ~ /^(exec|command|env|xargs|sudo)$/ || text ~ /^-/) continue
        if (text ~ /(^|\/)(find|find\.exe|gfind)$/) return 1
        shell = text ~ /^(sh|bash|dash|ksh)$/
        command = 0
    }
    return 0
}
function inspect_tokens(logical, raw, number, tokens, starts, ends, count) {
    if (logical ~ /find/ && contains_find(logical)) {
        print file ":" number ": findutils command is forbidden in build/test code"
        failed = 1
    }
}
# Python helpers may launch a literal command too. Skip comments and string
# data, then inspect only the first literal argument of process-launch calls.
function python_find(source,    i, ch, quote, width, first, literal, tail, launch, raw, limit) {
    limit = length(source)
    for (i = 1; i <= limit; i++) {
        ch = substr(source, i, 1)
        if (ch == "#") { while (i <= limit && substr(source, i, 1) != "\n") i++; continue }
        launch = 0
        if (ch == ".") {
            tail = substr(source, i)
            if (match(tail, /^\.(run|Popen|call|check_call|check_output|system)[[:space:]]*\([[:space:]]*[[(]?[[:space:]]*[ru]?/)) {
                i += RLENGTH; ch = substr(source, i, 1); launch = 1
            }
        }
        if (ch != "\047" && ch != "\"") continue
        quote = ch; width = 1; literal = ""
        if (substr(source, i, 3) == ch ch ch) width = 3
        i += width
        while (i <= limit) {
            if (substr(source, i, width) == (width == 3 ? quote quote quote : quote)) break
            ch = substr(source, i++, 1)
            if (ch == "\\" && i <= limit) {
                ch = substr(source, i++, 1)
                if (ch == "n") ch = "\n"
                else if (ch == "t") ch = "\t"
                else if (ch == "\n") ch = ""
            }
            literal = literal ch
        }
        i += width - 1
        if (launch && contains_find(literal)) return 1
    }
    return 0
}
function inspect_file(    line, text, literal) {
    if (!candidate) return
    reset_shell()
    if (file ~ /\.py$/ || input_language == "python") {
        text = ""
        for (line = 1; line <= line_count; line++) text = text lines[line] "\n"
        if (python_find(text)) {
            print file ": findutils command is forbidden in build/test code"
            failed = 1
        }
        return
    }
    for (line = 1; line <= line_count; line++) {
        # Automake source inventories are data, not shell programs. Only
        # recipes and literal $(shell ...) expressions introduce commands.
        if (file ~ /Makefile\.am$/ && pending == "" &&
            lines[line] !~ /^\t/ && lines[line] !~ /\$\(shell[[:space:]]/) continue
        if (file ~ /meson\.build$/) {
            text = lines[line]; sub(/#.*/, "", text)
            while (match(text, /\047([^\047\\]|\\.)*\047|"([^"\\]|\\.)*"/)) {
                literal = substr(text, RSTART, RLENGTH)
                text = substr(text, RSTART + RLENGTH)
                if (contains_find(unquote(literal))) {
                    print file ":" line ": findutils command is forbidden in build/test code"
                    failed = 1
                }
            }
        } else shell_line(lines[line] "\n", line)
    }
}
FNR == 1 {
    inspect_file(); file = FILENAME; line_count = candidate = 0
    relative = substr(file, length(root) + 2)
    eligible = (file == "" || file == "-" || file ~ /(^|\/)(Makefile\.am|meson\.build)$/ ||
        relative ~ /^tests\/.*\.(sh|t)$/ ||
        relative ~ /^tests\/_static\/python\/.*\.py$/ ||
        relative ~ /^build-aux\/.*\.sh(\.in)?$/ ||
        relative ~ /^tools\/(gen-amalgamation|update-mime-database)\.sh$/)
}
!eligible { next }
{
    lines[++line_count] = $0
    if ($0 ~ /(^|[^[:alnum:]_])(find|gfind)([^[:alnum:]_]|$)/) candidate = 1
}
END { inspect_file(); exit failed }
