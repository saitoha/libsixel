# Check source-job registration and generic corpus replay declarations.
function finish_job() {
    if (job == "") return
    if (harness != "" || corpus != "" || artifact != "") {
        generic_jobs++
        if (harness == "" || corpus == "" ||
            artifact !~ /^\.tmp\/fuzz-artifacts-[^[:space:]]+$/) {
            print job ": incomplete or uncollected corpus replay inputs" > "/dev/stderr"
            failed = 1
        }
    } else if (!(job in registered)) {
        print job ": missing triage registration" > "/dev/stderr"
        failed = 1
    }
}
FNR == NR {
    if ($0 ~ /def fuzzing_replay_entry\(/) dispatch = 1
    if (dispatch && /FUZZ_REPLAY_HARNESS/) generic_dispatch = 1
    if (dispatch && /^[[:space:]]*when /) {
        line = $0
        while (match(line, /\047fuzz_[[:alnum:]_]+\047/)) {
            registered[substr(line, RSTART + 1, RLENGTH - 2)] = 1
            line = substr(line, RSTART + RLENGTH)
        }
    }
    if (dispatch && /workflow_file = ENV/) dispatch = 0
    next
}
/^  [[:alnum:]_]+:$/ {
    finish_job()
    job = $1
    sub(/:$/, "", job)
    harness = corpus = artifact = ""
    if (job !~ /^fuzz_/) job = ""
    else jobs++
}
job != "" && /^      FUZZ_REPLAY_(HARNESS|CORPUS|ARTIFACT_DIR):/ {
    value = $0
    sub(/^[^:]+:[[:space:]]*/, "", value)
    gsub(/^[\047\042]|[\047\042]$/, "", value)
    if ($1 == "FUZZ_REPLAY_HARNESS:") harness = value
    if ($1 == "FUZZ_REPLAY_CORPUS:") corpus = value
    if ($1 == "FUZZ_REPLAY_ARTIFACT_DIR:") artifact = value
}
END {
    finish_job()
    if (!jobs || (generic_jobs && !generic_dispatch)) {
        print "missing fuzz jobs or generic corpus dispatcher" > "/dev/stderr"
        failed = 1
    }
    exit failed
}
