#!/bin/sh
# TAP test verifying quantize choices keep "name -> description" in --help.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

help_text=''
status=0

set +x
help_text=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --help 2>&1) || status=$?
set -x

test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load runtime help from img2sixel"
    exit 0
}

test -n "${help_text}" || {
    set +x
    status=0
    help_text=$("${IMG2SIXEL_PATH}" --help 2>&1) || status=$?
    set -x
}

test "${status}" -eq 0 || {
    echo "not ok" 1 - "fallback help read failed"
    exit 0
}

test -n "${help_text}" || {
    echo "not ok" 1 - "runtime help text is empty"
    exit 0
}

test "${help_text#*medoids -> k-medoids clustering. sub-option:*}" \
    != "${help_text}" || {
    echo "not ok" 1 - "medoids section heading is missing in runtime help"
    exit 0
}

test "${help_text#*:algo=NAME*choose*k-medoids*solver:*}" \
    != "${help_text}" || {
    echo "not ok" 1 - "medoids algo heading is missing in runtime help"
    exit 0
}

test "${help_text#*auto*adaptive*default*}" != "${help_text}" || {
    echo "not ok" 1 - "medoids auto description is missing in runtime help"
    exit 0
}

test "${help_text#*sample*CLARA:*}" != "${help_text}" || {
    echo "not ok" 1 - "medoids sample description is missing in runtime help"
    exit 0
}

test "${help_text#*random*CLARANS:*}" != "${help_text}" || {
    echo "not ok" 1 - "medoids random description is missing in runtime help"
    exit 0
}

test "${help_text#*bandit*BanditPAM:*}" != "${help_text}" || {
    echo "not ok" 1 - "medoids bandit description is missing in runtime help"
    exit 0
}

test "${help_text#*:inittype=TYPE*choose*k-means*seed*mode:*}" \
    != "${help_text}" || {
    echo "not ok" 1 - "kmeans inittype heading is missing in runtime help"
    exit 0
}

test "${help_text#*auto*choose*seed*mode*automatically*}" \
    != "${help_text}" || {
    echo "not ok" 1 - "kmeans inittype auto description is missing in runtime help"
    exit 0
}

echo "ok" 1 - "quantize choices keep name -> description runtime contract"
exit 0
