# Validate the job-level VM queue before hiding supported new syntax from
# actionlint versions which predate GitHub's concurrency.queue property.
function valid(group, queue, cancel) {
    return group == "libsixel-vm" && queue == "max" && cancel == "false"
}
function finish_job() {
    if ((vm || queue != "") &&
        (!valid(group, queue, cancel) || queues != 1)) {
        print "# " FILENAME ":" job ": VM jobs require the shared non-cancelling queue"
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
FNR == 1 { finish_job(); job = "" }
/^  [[:alnum:]_-]+:$/ { finish_job(); job = $1 }
/^    concurrency:$/ { concurrency = 1 }
/^    [^ ]/ && !/^    concurrency:$/ { concurrency = 0 }
concurrency && /^      group:/ { group = $2 }
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
