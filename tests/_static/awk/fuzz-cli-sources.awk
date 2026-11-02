# Compare literal source lists, including continued Automake assignments.
function consume(line, group,    n, word, i) {
    sub(/#.*/, "", line)
    sub(/^[^=]*=/, "", line)
    gsub(/\\/, " ", line)
    n = split(line, word)
    for (i = 1; i <= n; i++) {
        if (word[i] !~ /\.c$/) continue
        sub(/^.*\//, "", word[i])
        sources[group, word[i]] = 1
        members[word[i]] = 1
    }
}
FNR == 1 { active = 0 }
/^[[:space:]]*(img2sixel_SOURCES|FUZZ_IMG2SIXEL_EMBED_SRCS)[[:space:]]*=/ {
    active = 1
    group = ($0 ~ /^[[:space:]]*img2sixel_SOURCES/ ? "cli" : "fuzz")
    declarations[group]++
}
active { consume($0, group); active = ($0 ~ /\\[[:space:]]*$/) }
END {
    if (!declarations["cli"] || !declarations["fuzz"]) failed = 1
    for (name in members) {
        if (sources["cli", name] != sources["fuzz", name]) {
            print "embedded CLI fuzz source differs: " name
            failed = 1
        }
    }
    exit failed
}
