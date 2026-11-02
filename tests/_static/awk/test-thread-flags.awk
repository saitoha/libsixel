# Each conditional test-runner mode must retain the configured thread ABI.
/^[[:space:]]*TESTS_COMMON_CFLAGS_VAL[[:space:]]*=/ {
    active = 1; assignment = ""; first = FNR; count++
}
active {
    assignment = assignment " " $0
    if ($0 !~ /\\[[:space:]]*$/) {
        if (assignment !~ /(^|[[:space:]])@THREAD_CFLAGS@([[:space:]]|$)/) {
            print FILENAME ":" first ": test support lacks @THREAD_CFLAGS@"
            failed = 1
        }
        active = 0
    }
}
END { exit (failed || active || !count) }
