# Validate the job-level VM queue before hiding supported new syntax from
# actionlint versions which predate GitHub's concurrency.queue property.
function valid(group, queue, cancel) {
    return group == "libsixel-vm" && queue == "max" && cancel == "false"
}
function triage_route() {
    return "${{ (endsWith(matrix.exec_type, '-vm') || " \
        "matrix.exec_type == 'cross-platform-vms') && 'libsixel-vm' || " \
        "format('triage-job-{0}-{1}-{2}', github.run_id, " \
        "github.run_attempt, strategy.job-index) }}"
}
function finish_job() {
    if (previous_file ~ /(^|\/)triage\.yml$/ && job == "ci-steps:") {
        vm = 1
        group = group == triage_route() ? "libsixel-vm" : ""
    }
    if ((vm || queue != "") &&
        (!valid(group, queue, cancel) || queues != 1)) {
        print "# " previous_file ":" job ": VM jobs require the shared non-cancelling queue"
        failed = 1
    }
    group = queue = cancel = ""
    vm = concurrency = queues = 0
}
BEGIN {
    if (!valid("libsixel-vm", "max", "false") ||
        valid("other", "max", "false") ||
        valid("libsixel-vm", "single", "false") ||
        valid("libsixel-vm", "max", "true")) { failed = 1; exit 1 }
}
FNR == 1 { finish_job(); job = ""; previous_file = FILENAME }
/^  [[:alnum:]_-]+:$/ { finish_job(); job = $1 }
/^    concurrency:$/ { concurrency = 1 }
/^    [^ ]/ && !/^    concurrency:$/ { concurrency = 0 }
concurrency && /^      group:/ { group = substr($0, 14) }
concurrency && /^      queue:/ { queue = $2; queues++ }
concurrency && /^      cancel-in-progress:/ { cancel = $2 }
/^[[:space:]]*#/ { if (normalize) print; next }
/^[[:space:]]*queue:/ {
    if (!concurrency || $0 !~ /^      queue: max$/) {
        print "# " FILENAME ":" FNR ": unsupported concurrency queue"
        failed = 1
    }
    if (normalize) print ""
    next
}
/exec_type:.*vm/ || /uses:.*(vmactions\/|cross-platform-actions\/)/ { vm = 1 }
{ if (normalize) print }
END { finish_job(); exit failed }
