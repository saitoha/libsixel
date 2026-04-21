#!/bin/sh
# TAP test verifying quantize choices keep "name -> description" in source.

set -eux

test -f "${TOP_SRCDIR}/converters/img2sixel.c" || {
    printf "1..0 # SKIP missing converters/img2sixel.c\n"
    exit 0
}

echo "1..1"
set -v
set +xv

found_algo_header=0
found_algo_auto=0
found_algo_sample=0
found_algo_random=0
found_algo_bandit=0
found_inittype_header=0
found_inittype_auto=0

while IFS= read -r source_line || test -n "${source_line}"; do
    case "${source_line}" in
        *:algo=NAME*choose\ k-medoids\ solver:*)
            found_algo_header=1
            ;;
        *auto*adaptive\ default*)
            found_algo_auto=1
            ;;
        *sample*CLARA:*)
            found_algo_sample=1
            ;;
        *random*CLARANS:*)
            found_algo_random=1
            ;;
        *bandit*BanditPAM:*)
            found_algo_bandit=1
            ;;
        *:inittype=TYPE*choose\ k-means\ seed\ mode:*)
            found_inittype_header=1
            ;;
        *auto*choose\ seed\ mode\ automatically*)
            found_inittype_auto=1
            ;;
        *) ;;
    esac
done < "${TOP_SRCDIR}/converters/img2sixel.c"

test "${found_algo_header}" -eq 1 || {
    echo "not ok" 1 - "medoids algo heading is missing in source help"
    exit 0
}

test "${found_algo_auto}" -eq 1 || {
    echo "not ok" 1 - "medoids auto description is missing in source help"
    exit 0
}

test "${found_algo_sample}" -eq 1 || {
    echo "not ok" 1 - "medoids sample description is missing in source help"
    exit 0
}

test "${found_algo_random}" -eq 1 || {
    echo "not ok" 1 - "medoids random description is missing in source help"
    exit 0
}

test "${found_algo_bandit}" -eq 1 || {
    echo "not ok" 1 - "medoids bandit description is missing in source help"
    exit 0
}

test "${found_inittype_header}" -eq 1 || {
    echo "not ok" 1 - "kmeans inittype heading is missing in source help"
    exit 0
}

test "${found_inittype_auto}" -eq 1 || {
    echo "not ok" 1 - "kmeans inittype auto description is missing in source help"
    exit 0
}

echo "ok" 1 - "quantize choices keep name -> description source contract"
exit 0
