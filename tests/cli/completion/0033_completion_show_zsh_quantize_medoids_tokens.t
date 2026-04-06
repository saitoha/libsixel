#!/bin/sh
# TAP test verifying zsh completion exports medoids quantize tokens.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
set +x

status=0
msg=''
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

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
        -1 zsh
) || status=$?

test "${status}" = 0 || {
    echo "not ok" 1 - "zsh completion output failed"
    exit 0
}

case "${msg}" in
    *medoids*) medoids_found=1 ;;
esac
case "${msg}" in
    *algo=*) algo_found=1 ;;
esac
case "${msg}" in
    *bandit*) bandit_found=1 ;;
esac
case "${msg}" in
    *auto*) auto_found=1 ;;
esac
case "${msg}" in
    *seed=*) seed_found=1 ;;
esac
case "${msg}" in
    *iter=*) iter_found=1 ;;
esac
case "${msg}" in
    *sample=*) sample_found=1 ;;
esac
case "${msg}" in
    *histbits=*) histbits_found=1 ;;
esac
case "${msg}" in
    *point_budget=*) point_budget_found=1 ;;
esac
case "${msg}" in
    *rare_keep=*) rare_keep_found=1 ;;
esac
case "${msg}" in
    *prune_mass=*) prune_mass_found=1 ;;
esac
case "${msg}" in
    *clara_trials=*) clara_trials_found=1 ;;
esac
case "${msg}" in
    *clara_sample=*) clara_sample_found=1 ;;
esac
case "${msg}" in
    *clarans_local=*) clarans_local_found=1 ;;
esac
case "${msg}" in
    *clarans_neighbors=*) clarans_neighbors_found=1 ;;
esac
case "${msg}" in
    *bandit_iter=*) bandit_iter_found=1 ;;
esac
case "${msg}" in
    *bandit_candidates=*) bandit_candidates_found=1 ;;
esac
case "${msg}" in
    *bandit_batch=*) bandit_batch_found=1 ;;
esac

test "${medoids_found}" = 1 || {
    echo "not ok" 1 - "missing medoids base candidate in zsh completion"
    exit 0
}

test "${algo_found}" = 1 || {
    echo "not ok" 1 - "missing medoids algo key in zsh completion"
    exit 0
}

test "${bandit_found}" = 1 || {
    echo "not ok" 1 - "missing medoids algo values in zsh completion"
    exit 0
}

test "${auto_found}" = 1 || {
    echo "not ok" 1 - "missing medoids auto algo value in zsh completion"
    exit 0
}

test "${seed_found}" = 1 || {
    echo "not ok" 1 - "missing medoids seed key in zsh completion"
    exit 0
}

test "${iter_found}" = 1 || {
    echo "not ok" 1 - "missing medoids iter key in zsh completion"
    exit 0
}

test "${sample_found}" = 1 || {
    echo "not ok" 1 - "missing medoids sample key in zsh completion"
    exit 0
}

test "${histbits_found}" = 1 || {
    echo "not ok" 1 - "missing medoids histbits key in zsh completion"
    exit 0
}

test "${point_budget_found}" = 1 || {
    echo "not ok" 1 - "missing medoids point_budget key in zsh completion"
    exit 0
}

test "${rare_keep_found}" = 1 || {
    echo "not ok" 1 - "missing medoids rare_keep key in zsh completion"
    exit 0
}

test "${prune_mass_found}" = 1 || {
    echo "not ok" 1 - "missing medoids prune_mass key in zsh completion"
    exit 0
}

test "${clara_trials_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clara_trials key in zsh completion"
    exit 0
}

test "${clara_sample_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clara_sample key in zsh completion"
    exit 0
}

test "${clarans_local_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clarans_local key in zsh completion"
    exit 0
}

test "${clarans_neighbors_found}" = 1 || {
    echo "not ok" 1 - "missing medoids clarans_neighbors key in zsh completion"
    exit 0
}

test "${bandit_iter_found}" = 1 || {
    echo "not ok" 1 - "missing medoids bandit_iter key in zsh completion"
    exit 0
}

test "${bandit_candidates_found}" = 1 || {
    echo "not ok" 1 - "missing medoids bandit_candidates key in zsh completion"
    exit 0
}

test "${bandit_batch_found}" = 1 || {
    echo "not ok" 1 - "missing medoids bandit_batch key in zsh completion"
    exit 0
}

echo "ok" 1 - "zsh completion includes medoids quantize tokens"
exit 0
