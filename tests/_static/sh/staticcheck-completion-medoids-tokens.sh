#!/bin/sh
# Emit TAP for quantize completion token presence in source scripts.

set -eu

echo "1..1"

src_root=$1
bash_completion=$src_root/converters/shell-completion/bash/img2sixel
zsh_completion=$src_root/converters/shell-completion/zsh/_img2sixel

if test ! -f "$bash_completion" || test ! -f "$zsh_completion"; then
    echo "ok 1 # SKIP missing completion source script"
    exit 0
fi

bash_text=$(cat "$bash_completion")
zsh_text=$(cat "$zsh_completion")
status=0

for token in \
    medoids algo= seed= iter= sample= \
    histbits= point_budget= rare_keep= prune_mass= \
    auction= auction_shortlist= \
    clara_trials= clara_sample= clarans_local= clarans_neighbors= \
    bandit_iter= bandit_candidates= bandit_batch= \
    center fft swap hybrid restarts= \
    profile= auto_policy= auto_fft_threshold= space_policy= \
    candidate_policy= init_seeds= \
    rare_keep= budget_policy= budget_scale= \
    swap_topk= swap_update= swap_patience= swap_min_gain= \
    auto \
    pam sample random bandit fft swap hybrid \
    legacy speed balance quality adaptive perceptual full incremental
do
    case "$bash_text" in
        *"$token"*)
            ;;
        *)
            echo "# $bash_completion: missing token: $token"
            status=1
            ;;
    esac

    case "$zsh_text" in
        *"$token"*)
            ;;
        *)
            echo "# $zsh_completion: missing token: $token"
            status=1
            ;;
    esac
done

if test "$status" -eq 0; then
    echo "ok 1 - quantize completion tokens exist in source scripts"
    exit 0
fi

echo "not ok 1 - quantize completion tokens exist in source scripts"
exit 1
