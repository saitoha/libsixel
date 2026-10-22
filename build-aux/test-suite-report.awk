# Preserve failure details and report every test's wall time and slow cases.
function fatal(message) {
    print "test-suite-report: " message > "/dev/stderr"
    failed = 1
    exit 1
}
# Only generated integer counts cross back into the shell. Do not emit shell
# source containing test names or diagnostics from the result files.
function first_word(value,    words) {
    split(value, words, " ")
    return words[1]
}
function append_suite_log(path,    status, line) {
    # Probe readability here so missing diagnostics contribute to the counts.
    # The shell copies bytes: GNV awk adds a newline to an incomplete record,
    # and its system() does not follow POSIX quoting or stdout inheritance.
    status = getline line < path
    close(path)
    if (status < 0) {
        print "Hmissing log file: " path >> diagnostics_file
        print "H" >> diagnostics_file
        outcomes["ERROR"]++
    } else {
        print "F" path >> diagnostics_file
    }
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
    } else if (name in limits) {
        allowed[++allowed_count] = sprintf("ALLOW %.3f ms (limit %.3f): %s [%s]",
            duration, limit, name, result)
    }
}
BEGIN {
    if (suite) {
        if (diagnostics_file == "")
            fatal("suite mode requires the shell report driver")
        print "H" suite_title > diagnostics_file
        print "H" > diagnostics_file
    }
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
                field[2] + 0 <= 30 ||
                sprintf("%g", field[2] + 0) ~ /[Ii][Nn][Ff]|[Nn][Aa][Nn]/ ||
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
    if (suite) {
        if (base == "") next
        sub(/[.]trs$/, "", base)
        relative_name = base
        if (index(relative_name, builddir "/") == 1)
            relative_name = substr(relative_name, length(builddir) + 2)
        global_result = "RUN"
    }
    result = "RUN"
    duration = "unmeasured"
    name = base
    copy_log = 1
    batch = 0
    while ((status = getline line < (base ".trs")) > 0) {
        sub(/\r$/, "", line)
        if (suite && line ~ /^:test-result:/) {
            value = line
            sub(/^:test-result:/, "", value)
            outcomes[first_word(value)]++
            total++
        }
        if (suite && line ~ /^:global-test-result:/) {
            value = line
            sub(/^:global-test-result:/, "", value)
            global_result = first_word(value)
            if (global_result == "") global_result = "RUN"
        }
        if (suite && line ~ /^:copy-in-global-log:/) {
            value = line
            sub(/^:copy-in-global-log:/, "", value)
            if (tolower(first_word(value)) == "no") copy_log = 0
        }
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
        } else if (!suite && line ~ /^:copy-in-global-log: [nN][oO]/) {
            copy_log = 0
        }
    }
    if (status < 0) fatal("cannot read " base ".trs")
    close(base ".trs")
    add_timing(name, duration, batch ? "BATCH" : result)
    if (suite) {
        if (global_result == "FAIL" || global_result == "XPASS" ||
            global_result == "ERROR")
            print global_result ": " relative_name >> failures_file
        if (copy_log) {
            print "H" global_result ": " relative_name >> diagnostics_file
            append_suite_log(base ".log")
        }
    } else if (copy_log && !times_only) {
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
    if (suite) {
        # Keep the report footer and shell handoff in their established order.
        print "TOTAL: " (total + 0)
        print "PASS: " (outcomes["PASS"] + 0)
        print "SKIP: " (outcomes["SKIP"] + 0)
        print "XFAIL: " (outcomes["XFAIL"] + 0)
        print "FAIL: " (outcomes["FAIL"] + 0)
        print "XPASS: " (outcomes["XPASS"] + 0)
        print "ERROR: " (outcomes["ERROR"] + 0)
        printf "%d %d %d %d %d %d %d\n", total, outcomes["PASS"], \
            outcomes["SKIP"], outcomes["XFAIL"], outcomes["FAIL"], \
            outcomes["XPASS"], outcomes["ERROR"] > counts_file
        if (close(counts_file) != 0) fatal("cannot write " counts_file)
        close(failures_file)
        if (close(diagnostics_file) != 0)
            fatal("cannot write " diagnostics_file)
    }
    if (!from_report) {
        print "Test durations (wall time, including process startup)"
        print "===================================================="
        print ""
        for (i = 1; i <= count; i++) print timings[i]
        print ""
    }
    summary = sprintf("Slow tests: %d; slow setup/batch processes: %d (default 30 ms; explicit per-test exception budgets).",
                      slow_tests, slow_context)
    print summary
    if (!from_report && (!quiet || functional_failure))
        print summary > "/dev/stderr"
    for (i = 1; i <= allowed_count; i++) {
        print allowed[i]
        if (!from_report && (!quiet || functional_failure))
            print allowed[i] > "/dev/stderr"
    }
    for (i = 1; i <= slow_count; i++) {
        print slow[i]
        if (!from_report && (!quiet || functional_failure))
            print slow[i] > "/dev/stderr"
    }
}
