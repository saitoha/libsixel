#!/usr/bin/env bash
#
# Classify DragonFlyBSD CI failures from GitHub Actions job logs.
#
# Usage:
#   classify-dragonfly-failure.sh --job-id <id> [--repo <owner/repo>]
#   classify-dragonfly-failure.sh --run-id <id> --job-name <name> [--repo <owner/repo>]
#
# Output:
#   line 1: classification code
#   line 2: evidence line

set -euo pipefail

usage() {
    cat >&2 <<'USAGE'
usage: classify-dragonfly-failure.sh \
  (--job-id <id> | --run-id <id> --job-name <matrix.label>) \
  [--repo <owner/repo>]
USAGE
}

derive_repo_from_origin() {
    local remote_url
    local repo

    remote_url="$(git config --get remote.origin.url 2>/dev/null || true)"
    if test -z "$remote_url"; then
        return 1
    fi

    repo="${remote_url##*github.com[:/]}"
    repo="${repo%.git}"
    if test "$repo" = "$remote_url" || test -z "$repo"; then
        return 1
    fi

    printf '%s\n' "$repo"
}

extract_first_fixed_line() {
    local haystack
    local needle

    haystack="$1"
    needle="$2"
    grep -m 1 -F "$needle" <<<"$haystack" || true
}

extract_first_regex_line() {
    local haystack
    local pattern

    haystack="$1"
    pattern="$2"
    grep -m 1 -E "$pattern" <<<"$haystack" || true
}

fetch_job_conclusion() {
    local job_id_arg

    job_id_arg="$1"
    gh api "/repos/$repo/actions/jobs/$job_id_arg" \
        --jq '.conclusion // ""' 2>/dev/null || true
}

fetch_job_log() {
    local job_id_arg

    job_id_arg="$1"
    gh api "/repos/$repo/actions/jobs/$job_id_arg/logs" 2>/dev/null || true
}

run_id=""
job_name=""
job_id=""
repo=""

while test "$#" -gt 0; do
    case "$1" in
        --job-id)
            job_id="${2-}"
            shift 2
            ;;
        --run-id)
            run_id="${2-}"
            shift 2
            ;;
        --job-name)
            job_name="${2-}"
            shift 2
            ;;
        --repo)
            repo="${2-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage
            exit 2
            ;;
    esac
done

if test -z "$job_id" && { test -z "$run_id" || test -z "$job_name"; }; then
    echo "error: --job-id or --run-id plus --job-name is required" >&2
    usage
    exit 2
fi

if test -z "$repo"; then
    repo="$(derive_repo_from_origin || true)"
fi
if test -z "$repo"; then
    echo "error: failed to determine repository. pass --repo <owner/repo>" >&2
    exit 2
fi

if ! command -v gh >/dev/null 2>&1; then
    echo "error: gh command is required" >&2
    exit 2
fi

if test -z "$job_id"; then
    job_id="$(
        gh run view "$run_id" --repo "$repo" --json jobs \
            --jq "[.jobs[] | select(.name == \"$job_name\")][0].databaseId // \"\"" \
            2>/dev/null || true
    )"
    job_conclusion="$(
        gh run view "$run_id" --repo "$repo" --json jobs \
            --jq "[.jobs[] | select(.name == \"$job_name\")][0].conclusion // \"\"" \
            2>/dev/null || true
    )"
else
    job_conclusion="$(fetch_job_conclusion "$job_id")"
fi

if test -z "$job_id"; then
    printf '%s\n' "UNKNOWN"
    printf 'evidence: %s\n' "job not found: $job_name"
    exit 0
fi

log_text=""
if test -n "$run_id"; then
    log_text="$(
        gh run view "$run_id" --repo "$repo" --job "$job_id" --log \
            2>/dev/null || true
    )"
fi
if test -z "$log_text"; then
    log_text="$(fetch_job_log "$job_id")"
fi
if test -z "$log_text"; then
    printf '%s\n' "UNKNOWN"
    printf 'evidence: %s\n' "job log is unavailable for job_id=$job_id"
    exit 0
fi

if test "$job_conclusion" = "success"; then
    printf '%s\n' "UNKNOWN"
    printf 'evidence: %s\n' "job conclusion is success"
    exit 0
fi

classification="UNKNOWN"
evidence=""

if grep -Fq "Boot timed out after retry. Giving up." <<<"$log_text" \
    || grep -Fq "Boot timed out after 5 minutes. Killing QEMU and retrying..." <<<"$log_text"; then
    classification="VM_SETUP_FAILURE"
    evidence="$(extract_first_regex_line "$log_text" "Boot timed out after retry\\. Giving up\\.|Boot timed out after 5 minutes\\. Killing QEMU and retrying\\.\\.\\.")"
elif grep -Fq "__DFBSD_PHASE_RUN__" <<<"$log_text"; then
    classification="BUILD_OR_TEST_FAILURE"
    evidence="$(extract_first_regex_line "$log_text" "__DFBSD_PHASE_RUN__|##\\[error\\].*")"
elif grep -Eq "##\\[error\\]The process '/usr/bin/(ssh|python3)' failed" <<<"$log_text"; then
    classification="VM_SETUP_OR_PREPARE_FAILURE"
    evidence="$(extract_first_regex_line "$log_text" "##\\[error\\]The process '/usr/bin/(ssh|python3)' failed")"
else
    classification="UNKNOWN"
    evidence="$(extract_first_regex_line "$log_text" "##\\[error\\].*")"
fi

if test -z "$evidence"; then
    evidence="no matching signature line found"
fi

printf '%s\n' "$classification"
printf 'evidence: %s\n' "$evidence"
