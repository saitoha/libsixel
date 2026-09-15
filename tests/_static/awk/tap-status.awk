# SPDX-License-Identifier: MIT
# Evaluate one flat staticcheck TAP stream from stdout only. Diagnostics on
# stderr never enter this parser. Static invariants require a plan and cannot
# be waived with TODO; the ordinary behavioral TAP harness is independent.
function problem(message) {
    print "TAP error at line " NR ": " message
    failed = 1
}
function number(text) {
    # All supported awk implementations represent these integers exactly.
    return length(text) <= 10 && text + 0 <= 2147483647
}
{
    sub(/\r$/, "")
    if (yaml) {
        if ($0 ~ /^[ \t]+\.\.\.[ \t]*$/) {
            yaml = 0
        } else if ($0 !~ /^[ \t]/) {
            problem("unterminated diagnostic block")
            yaml = 0
        } else {
            next
        }
        if (!yaml && $0 ~ /^[ \t]+\.\.\./) next
    }
    if ($0 ~ /^[ \t]*$/ || $0 ~ /^#/) next
    if ($0 ~ /^[ \t]+---[ \t]*$/) {
        if (!after_test) problem("diagnostic block without test")
        yaml = 1
        next
    }
    if ($0 ~ /^Bail out!/) {
        problem($0)
        bailed = 1
        next
    }
    if (bailed) {
        problem("result after bailout")
        next
    }
    if ($0 ~ /^TAP version/) {
        if (records || $0 != "TAP version 13") {
            problem("invalid or misplaced TAP version")
        }
        records++
        next
    }
    if ($0 ~ /^1\.\./) {
        if ($0 !~ /^1\.\.[0-9]+([ \t]*#.*)?[ \t]*$/) {
            problem("malformed plan")
            next
        }
        text = $0
        sub(/^1\.\./, "", text)
        sub(/[ \t#].*$/, "", text)
        if (!number(text)) problem("plan outside supported integer range")
        if (saw_plan) problem("duplicate plan")
        planned = text + 0
        saw_plan = 1
        trailing_plan = seen > 0
        directive = $0
        sub(/^[^#]*#?[ \t]*/, "", directive)
        if (planned == 0 && tolower(directive) !~ /^skip([ \t:]|$)/) {
            problem("empty plan without SKIP")
        }
        if (planned > 0 && directive != "") {
            problem("directive on nonempty plan")
        }
        after_test = 0
        records++
        next
    }
    if ($0 ~ /^(not[ \t]+)?ok([ \t]|$)/) {
        if (trailing_plan) problem("test after trailing plan")
        if (saw_plan && planned == 0) problem("test after skip-all plan")
        negative = $0 ~ /^not[ \t]+ok([ \t]|$)/
        text = $0
        sub(/^(not[ \t]+)?ok[ \t]*/, "", text)
        seen++
        if (text ~ /^[0-9]/) {
            numeric = text
            sub(/[ \t#].*$/, "", numeric)
            if (numeric !~ /^[0-9]+$/ || !number(numeric) ||
                    numeric + 0 != seen) {
                problem("test number is not the next expected number")
            }
        }
        directive = text
        sub(/^[^#]*#?[ \t]*/, "", directive)
        directive = tolower(directive)
        if (directive ~ /^todo([ \t]|$)/) {
            problem("TODO does not establish a static invariant")
        } else if (directive ~ /^skip([ \t:]|$)/) {
            skipped++
        } else if (negative) {
            problem("failed test " seen ": " $0)
        }
        after_test = 1
        records++
        next
    }
    problem("unrecognized stdout record: " $0)
    after_test = 0
}
END {
    if (yaml) problem("unterminated diagnostic block")
    if (!saw_plan) problem("missing plan")
    if (saw_plan && seen != planned) problem("plan/result count mismatch")
    if (failed) exit 1
    if (planned == 0 || seen == skipped) exit 77
    exit 0
}
