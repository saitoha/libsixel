# Convert the source inventory to a header without passing it through argv.
/^TEST_RUNNER_AMALGAMATION_DEFINES[[:space:]]*=/ { active = 1 }
active {
    for (i = 1; i <= NF; ++i) {
        if ($i ~ /^-DBUILD_[A-Z0-9_]+$/ && !seen[$i]++) {
            print "#define " substr($i, 3) " 1"
            count++
        }
    }
    if ($0 !~ /\\[[:space:]]*$/) active = 0
}
END { if (!count) exit 1 }
