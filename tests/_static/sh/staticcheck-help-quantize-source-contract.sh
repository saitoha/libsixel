#!/bin/sh
# Emit TAP for quantize help source contracts in converters/img2sixel.c.

set -eu

echo "1..1"

src_root=$1
source_file=$src_root/converters/img2sixel.c
help_block=''
status=0
found_algo_header=0
found_algo_auto=0
found_algo_sample=0
found_algo_random=0
found_algo_bandit=0
found_inittype_header=0
found_inittype_auto=0
source_line=''

if test ! -f "$source_file"; then
    echo "ok 1 # SKIP missing converters/img2sixel.c"
    exit 0
fi

help_block=$(cat "$source_file" 2>/dev/null) || status=$?

test "$status" -eq 0 || {
    echo "not ok 1 - failed to read converters/img2sixel.c"
    exit 0
}

test "${help_block#*kmeans   -> k-means clustering. sub-option:*:inittype=TYPE*}" \
    != "${help_block}" || {
    echo "not ok 1 - missing kmeans suboption block in source help"
    exit 0
}

test "${help_block#*medoids -> k-medoids clustering. sub-option:*:algo=NAME*}" \
    != "${help_block}" || {
    echo "not ok 1 - missing medoids suboption block in source help"
    exit 0
}

test "${help_block#*center  -> discrete k-center clustering. sub-option:*:algo=NAME*}" \
    != "${help_block}" || {
    echo "not ok 1 - missing center suboption block in source help"
    exit 0
}

while IFS= read -r source_line || test -n "$source_line"; do
    case "$source_line" in
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
done < "$source_file"

test "$found_algo_header" -eq 1 || {
    echo "not ok 1 - medoids algo heading is missing in source help"
    exit 0
}

test "$found_algo_auto" -eq 1 || {
    echo "not ok 1 - medoids auto description is missing in source help"
    exit 0
}

test "$found_algo_sample" -eq 1 || {
    echo "not ok 1 - medoids sample description is missing in source help"
    exit 0
}

test "$found_algo_random" -eq 1 || {
    echo "not ok 1 - medoids random description is missing in source help"
    exit 0
}

test "$found_algo_bandit" -eq 1 || {
    echo "not ok 1 - medoids bandit description is missing in source help"
    exit 0
}

test "$found_inittype_header" -eq 1 || {
    echo "not ok 1 - kmeans inittype heading is missing in source help"
    exit 0
}

test "$found_inittype_auto" -eq 1 || {
    echo "not ok 1 - kmeans inittype auto description is missing in source help"
    exit 0
}

echo "ok 1 - quantize source help contract is intact"
exit 0
