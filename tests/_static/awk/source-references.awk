# Share full-file reads without weakening reference placement or syntax checks.
function collect(kind, links, errors,    line, expression, reference) {
    line = $0
    expression = kind ":[[:space:]]+docs/[A-Za-z0-9_./-]+\\.md"
    while (match(line, expression)) {
        if (FNR > 20 ||
            line !~ ("^[[:space:]]*(#|//|\\*)[[:space:]]*" kind ":") ||
            (test_path ~ /\.ps1$/ &&
             line !~ ("^[[:space:]]*#[[:space:]]*" kind ":"))) {
            print test_path ": " kind " reference must be a source " \
                "comment within the first 20 lines" >> errors
        }
        reference = substr(line, RSTART, RLENGTH)
        sub("^" kind ":[[:space:]]+", "", reference)
        print reference "|" test_path >> links
        line = substr(line, RSTART + RLENGTH)
    }
}
FNR == 1 {
    test_path = substr(FILENAME, length(src_root) + 1)
}
scope == "all" && FILENAME ~ /(\.t|\/[0-9][0-9][0-9][0-9]_[A-Za-z0-9_-]+\.sh)$/ && /^# Staticcheck: / {
    if (FNR > 20 || NF != 3 || $3 !~ /^staticcheck-[a-z0-9-]+$/) {
        print FILENAME ": invalid Staticcheck registration" > "/dev/stderr"
        exit 1
    }
    print $3 "|" FILENAME >> (output_root "/source-contracts")
}
(scope == "all" || scope == "policy") && index($0, "Policy:") {
    collect("Policy", output_root "/policy-links", output_root "/policy-errors")
}
(scope == "all" || scope == "test-plan") && index($0, "Test-plan:") {
    collect("Test-plan", output_root "/test-plan-links",
            output_root "/test-plan-errors")
}
