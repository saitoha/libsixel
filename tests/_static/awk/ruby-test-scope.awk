# Ruby is reserved for binding tests and their interpreter/coverage support.
function report_ruby(line) {
    print FILENAME ":" line ": Ruby is only allowed for binding tests" > "/dev/stderr"
    failed = 1
}
function inspect_tokens(logical, raw, line, tokens, starts, ends, count,    i, word) {
    for (i = 1; i <= count; i++) {
        word = tolower(unquote(tokens[i]))
        sub(/^.*\//, "", word)
        if (word ~ /^(rubyw?|jruby|truffleruby)([0-9]+(\.[0-9]+)*)?(\.exe)?$/ ||
            word ~ /^\$\{?(ruby|sixel_test_ruby)([}_]|$)/) {
            report_ruby(line)
            return
        }
    }
}
FNR == 1 {
    reset_shell()
    binding = FILENAME ~ /\/tests\/bindings\/ruby\// ||
        FILENAME ~ /\/tests\/_support\/bindings\// ||
        FILENAME ~ /\/staticcheck-binding-constants-sync\.sh$/ ||
        FILENAME ~ /\/meson-python-tap-wrapper\.sh$/
    ruby_source = FILENAME ~ /\.rb$/
    if (!binding && (ruby_source || $0 ~ /^#!.*ruby/)) report_ruby(1)
}
!binding && !ruby_source { shell_line($0 "\n", FNR) }
END { exit failed }
