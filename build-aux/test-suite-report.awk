# Preserve failure details and report every test's wall time and slow cases.
function fatal(message) {
    print "test-suite-report: " message > "/dev/stderr"
    failed = 1
    exit 1
}
function add_timing(name, duration, result,    limit, row) {
    if (duration == "unmeasured" && result != "SKIP")
        fatal("missing duration: " name)
    if (duration != "unmeasured" &&
        duration !~ /^[0-9]+([.][0-9]+)?$/)
        fatal("invalid duration: " name)
    row = "test-time: " duration " ms " result " " name
    timings[++count] = row
    if (duration == "unmeasured") return
    limit = (name in limits) ? limits[name] : 30
    if (duration + 0 > limit) {
        slow[++slow_count] = sprintf("SLOW %.3f ms (limit %.3f): %s [%s]",
                                    duration, limit, name, result)
        if (result == "SETUP" || result == "BATCH") slow_context++
        else slow_tests++
    }
}
BEGIN {
    if (allowlist != "") {
        status = getline line < allowlist
        sub(/\r$/, "", line)
        if (status <= 0 || line != "test\tlimit_ms\treason\tapproval")
            fatal("invalid allowlist header: " allowlist)
        while ((status = getline line < allowlist) > 0) {
            sub(/\r$/, "", line)
            fields = split(line, field, "\t")
            name = field[1]
            if (fields != 4 || name == "" || name ~ /^[ \t]|[ \t]$/ ||
                index(name, "*") || index(name, "?") ||
                index(name, "[") || index(name, "]") || name in limits ||
                field[2] !~ /^[0-9]+([.][0-9]+)?$/ ||
                field[2] + 0 <= 30 || field[2] + 0 > 100 ||
                field[3] !~ /[^ \t]/ || field[4] !~ /[^ \t]/)
                fatal("invalid allowlist entry: " line)
            limits[name] = field[2] + 0
        }
        if (status < 0) fatal("cannot read " allowlist)
        close(allowlist)
    }
}
{
    if (from_report) {
        if ($0 ~ /^test-time: /) {
            value = $0
            sub(/\r$/, "", value)
            split(value, field, " ")
            sub(/^test-time: [^ ]+ ms [^ ]+ /, "", value)
            add_timing(value, field[2], field[4])
        }
        next
    }
    base = $0
    sub(/\r$/, "", base)
    result = "RUN"
    duration = "unmeasured"
    name = base
    copy_log = 1
    batch = 0
    while ((status = getline line < (base ".trs")) > 0) {
        sub(/\r$/, "", line)
        if (line ~ /^:global-test-result: /) {
            result = substr(line, 22)
            if (result == "FAIL" || result == "ERROR" || result == "XPASS")
                functional_failure = 1
        } else if (line ~ /^:duration-ms: /) {
            duration = substr(line, 15)
        } else if (line ~ /^:test-name: /) {
            name = substr(line, 13)
        } else if (line ~ /^:subtest-time: /) {
            value = substr(line, 16)
            split(value, field, " ")
            sub(/^[^ ]+ ms [^ ]+ /, "", value)
            add_timing(value, field[1], field[3])
            batch = 1
        } else if (line ~ /^:copy-in-global-log: [nN][oO]/) {
            copy_log = 0
        }
    }
    if (status < 0) fatal("cannot read " base ".trs")
    close(base ".trs")
    add_timing(name, duration, batch ? "BATCH" : result)
    if (copy_log && !times_only) {
        header = result ": " base
        print header
        for (i = 0; i < length(header); i++) printf "="
        printf "\n\n"
        while ((status = getline line < (base ".log")) > 0) print line
        if (status < 0) fatal("cannot read " base ".log")
        close(base ".log")
        print ""
    }
}
END {
    if (failed) exit 1
    if (from_report && count == 0) fatal("no recorded test durations")
    if (!from_report) {
        print "Test durations (wall time, including process startup)"
        print "===================================================="
        print ""
        for (i = 1; i <= count; i++) print timings[i]
        print ""
    }
    summary = sprintf("Slow tests: %d; slow setup/batch processes: %d (default 30 ms; approved exceptions at most 100 ms).",
                      slow_tests, slow_context)
    print summary
    if (!from_report && (!quiet || functional_failure))
        print summary > "/dev/stderr"
    for (i = 1; i <= slow_count; i++) {
        print slow[i]
        if (!from_report && (!quiet || functional_failure))
            print slow[i] > "/dev/stderr"
    }
}
