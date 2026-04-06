#!/bin/sh
# TAP test verifying bash completion exports medoids quantize tokens.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
set +x

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

status=0
output_file="${ARTIFACT_LOCAL_DIR}/completion-medoids-bash.txt"
medoids_found=0
algo_found=0
bandit_found=0
auto_found=0
seed_found=0
iter_found=0
sample_found=0
histbits_found=0
point_budget_found=0
rare_keep_found=0
prune_mass_found=0
clara_trials_found=0
clara_sample_found=0
clarans_local_found=0
clarans_neighbors_found=0
bandit_iter_found=0
bandit_candidates_found=0
bandit_batch_found=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
    -1 bash > "${output_file}" || status=$?

test "${status}" = 0 || {
    echo "not ok" 1 - "bash completion output failed"
    exit 0
}

while IFS= read -r line; do
    case "${line}" in
        *medoids*) medoids_found=1 ;;
    esac
    case "${line}" in
        *algo=*) algo_found=1 ;;
    esac
    case "${line}" in
        *bandit*) bandit_found=1 ;;
    esac
    case "${line}" in
        *auto*) auto_found=1 ;;
    esac
    case "${line}" in
        *seed=*) seed_found=1 ;;
    esac
    case "${line}" in
        *iter=*) iter_found=1 ;;
    esac
    case "${line}" in
        *sample=*) sample_found=1 ;;
    esac
    case "${line}" in
        *histbits=*) histbits_found=1 ;;
    esac
    case "${line}" in
        *point_budget=*) point_budget_found=1 ;;
    esac
    case "${line}" in
        *rare_keep=*) rare_keep_found=1 ;;
    esac
    case "${line}" in
        *prune_mass=*) prune_mass_found=1 ;;
    esac
    case "${line}" in
        *clara_trials=*) clara_trials_found=1 ;;
    esac
    case "${line}" in
        *clara_sample=*) clara_sample_found=1 ;;
    esac
    case "${line}" in
        *clarans_local=*) clarans_local_found=1 ;;
    esac
    case "${line}" in
        *clarans_neighbors=*) clarans_neighbors_found=1 ;;
    esac
    case "${line}" in
        *bandit_iter=*) bandit_iter_found=1 ;;
    esac
    case "${line}" in
        *bandit_candidates=*) bandit_candidates_found=1 ;;
    esac
    case "${line}" in
        *bandit_batch=*) bandit_batch_found=1 ;;
    esac
done < "${output_file}"

test "${medoids_found}" = 1 || {
    echo "not ok" 1 - "missing medoids base candidate in bash completion"
    exit 0
}

test "${algo_found}" = 1 || {
    echo "not ok" 1 - "missing medoids algo key in bash completion"
    exit 0
}

test "${bandit_found}" = 1 || {
    echo "not ok" 1 - "missing medoids algo values in bash completion"
    exit 0
}

test "${auto_found}" = 1 || {
    echo "not ok" 1 - "missing medoids auto algo value in bash completion"
    exit 0
}

test "${seed_found}" = 1 || {
    echo "not ok" 1 - "missing medoids seed key in bash completion"
    exit 0
}

test "${iter_found}" = 1 || {
    echo "not ok" 1 - "missing medoids iter key in bash completion"
    exit 0
}

test "${sample_found}" = 1 || {
    echo "not ok" 1 - "missing medoids sample key in bash completion"
    exit 0
}

test "${histbits_found}" = 1 || {
    echo "not ok" 1 - "missing medoids histbits key in bash completion"
    exit 0
}

test "${point_budget_found}" = 1 || {
    echo "not ok" 1 - "missing medoids point_budget key in bash completion"
    exit 0
}

test "${rare_keep_found}" = 1 || {
    echo "not ok" 1 - "missing medoids rare_keep key in bash completion"
    exit 0
}

test "${prune_mass_found}" = 1 || {
    echo "not ok" 1 - "missing medoids prune_mass key in bash completion"
    exit 0
}

test "${clara_trials_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clara_trials key in bash completion"
    exit 0
}

test "${clara_sample_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clara_sample key in bash completion"
    exit 0
}

test "${clarans_local_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clarans_local key in bash completion"
    exit 0
}

test "${clarans_neighbors_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clarans_neighbors key in bash completion"
    exit 0
}

test "${bandit_iter_found}" = 1 || {
    echo "not ok" 1 - "missing medoids bandit_iter key in bash completion"
    exit 0
}

test "${bandit_candidates_found}" = 1 || {
    echo "not ok" 1 - "missing medoids bandit_candidates key in bash completion"
    exit 0
}

test "${bandit_batch_found}" = 1 || {
    echo "not ok" 1 - "missing medoids bandit_batch key in bash completion"
    exit 0
}

echo "ok" 1 - "bash completion includes medoids quantize tokens"
exit 0
