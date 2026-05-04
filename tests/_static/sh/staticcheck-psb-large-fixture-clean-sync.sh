#!/bin/sh
# Emit TAP for generated large fixture cleanup coverage.

set -eu

src_root=$1
prepare_script=$src_root/tests/_static/sh/prepare-psb-large-fixtures.sh
makefile_am=$src_root/tests/Makefile.am

echo "1..1"

test -f "$prepare_script" || {
    echo "not ok 1 - psb large fixture clean sync"
    echo "# missing fixture preparation script: $prepare_script"
    exit 1
}

test -f "$makefile_am" || {
    echo "not ok 1 - psb large fixture clean sync"
    echo "# missing tests Makefile.am: $makefile_am"
    exit 1
}

if awk '
BEGIN {
    in_webp_list = 0
}
FILENAME == ARGV[1] {
    if ($0 ~ /^webp_large_fixtures="/) {
        in_webp_list = 1
    }
    if (in_webp_list) {
        line = $0
        gsub(/["\\]/, " ", line)
        count = split(line, tokens, /[[:space:]]+/)
        for (i = 1; i <= count; i++) {
            if (tokens[i] ~ /^[A-Za-z0-9_.-]+\.webp$/) {
                fixtures[tokens[i]] = 1
            }
        }
        if ($0 ~ /"$/) {
            in_webp_list = 0
        }
    }
    next
}
FILENAME == ARGV[2] {
    if ($0 ~ /^clean-local:/) {
        in_clean_local = 1
        next
    }
    if (in_clean_local && $0 !~ /^\t/ && $0 !~ /^[[:space:]]*$/) {
        in_clean_local = 0
    }
    if (!in_clean_local) {
        next
    }
    for (fixture in fixtures) {
        if (index($0, fixture) != 0) {
            cleaned[fixture] = 1
        }
    }
    next
}
END {
    bad = 0
    found = 0
    for (fixture in fixtures) {
        found = 1
        if (!(fixture in cleaned)) {
            printf "# missing clean-local removal for %s\n", fixture
            bad = 1
        }
    }
    if (!found) {
        print "# no WebP large fixtures found in prepare script"
        bad = 1
    }
    exit bad ? 1 : 0
}
' "$prepare_script" "$makefile_am"; then
    echo "ok 1 - psb large fixture clean sync"
else
    echo "not ok 1 - psb large fixture clean sync"
    exit 1
fi
