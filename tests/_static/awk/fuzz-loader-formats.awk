# Read the workflow matrix and each build's literal format inventory.
function consume(line,    n, word, i) {
    sub(/#.*/, "", line)
    gsub(/[][,'"\\]/, " ", line)
    n = split(line, word)
    for (i = 1; i <= n; i++) {
        if (word[i] == "") continue
        formats[group, word[i]] = 1
        members[word[i]] = 1
    }
}
FNR == 1 { active = 0; job = 0 }
/^[[:space:]]*FUZZ_BUILTIN_FORMATS[[:space:]]*=/ {
    group = "Makefile inventory " ++make_count; groups[group] = 1
    line = $0; sub(/^[^=]*=/, "", line)
    consume(line); active = ($0 ~ /\\[[:space:]]*$/); next
}
/^[[:space:]]*fuzz_loader_formats[[:space:]]*=/ {
    group = "Meson inventory"; groups[group] = 1
    line = $0; sub(/^[^=]*=/, "", line)
    consume(line); active = ($0 !~ /\]/); next
}
/^  fuzz_push_loader_formats:/ { job = 1; next }
/^  [[:alnum:]_]+:/ { job = 0 }
job && /^[[:space:]]*target:[[:space:]]*\[/ {
    group = "workflow"; groups[group] = 1
    line = $0; sub(/^[^:]*:/, "", line); consume(line); next
}
active { consume($0); active = ($0 ~ /\\[[:space:]]*$/ || ($0 !~ /\]/ && group == "Meson inventory")) }
END {
    for (group in groups) count++
    if (count != 4 || !groups["workflow"]) failed = 1
    for (name in members) for (group in groups) {
        if (formats[group, name] != formats["workflow", name]) {
            print group ": loader format differs from workflow: " name
            failed = 1
        }
    }
    exit failed
}
