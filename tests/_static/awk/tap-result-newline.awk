# Check direct TAP emitters without executing shell code or expansions.
function is_result(word) {
    return unquote(word) ~ /^(ok|not ok)([[:space:]]|$)/
}
function can_start_command(previous) {
    return previous == "" || previous ~ /^([;|&()]+|[!{])$/ ||
        previous ~ /^(command|builtin|if|then|elif|else|do)$/ ||
        previous ~ /^[a-zA-Z_][a-zA-Z_0-9]*=/
}
function inspect_tokens(logical, raw, line, tokens, starts, ends, count,
                        i, command, arg, format, j) {
    for (i = 1; i < count; i++) {
        if (!can_start_command(tokens[i - 1])) continue
        command = unquote(tokens[i])
        if (command != "echo" && command != "printf") continue
        arg = i + 1
        if (command == "echo" && tokens[arg] ~ /^-[neE]+$/) arg++
        if (command == "printf" && tokens[arg] == "--") arg++
        if (is_result(tokens[arg]) ||
            (unquote(tokens[arg]) == "not" &&
             unquote(tokens[arg + 1]) == "ok") ||
            (command == "printf" && tokens[arg] ~ /^["']?(ok|not ok)\\n/)) {
            violation = 1
            break
        }
        if (command != "printf") continue
        format = tokens[arg]
        if (format !~ /^["']?%[bs]/) continue
        for (j = arg + 1; j <= count && tokens[j] !~ /^[;|&()<>]+$/; j++) {
            if (is_result(tokens[j])) {
                violation = 1
                break
            }
        }
        if (violation) break
    }
    if (violation) {
        print source ":" line ": TAP result must start with a newline"
        violation = 0
        failed = 1
    }
}
{
    source = $0
    reset_shell()
    number = 0
    while ((status = (getline text < source)) > 0)
        shell_line(text "\n", ++number)
    if (status < 0) {
        print source ": cannot read TAP source"
        failed = 1
    }
    close(source)
}
END { exit failed }
