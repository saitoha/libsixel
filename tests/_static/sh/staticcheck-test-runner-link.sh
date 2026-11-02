#!/bin/sh
# Validate link recipes, object lists and archive prerequisites without C.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
# Meson graph: docs/build/meson.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-21
set -eux
echo '1..1'
set -v

src_root=$1
checker=$src_root/tests/_static/awk/test-runner-meson-link.awk
awk -f "$checker" "$src_root/tests/meson.build" >&2 || {
    echo 'not ok 1 - Meson test runner archive inputs'
    exit 0
}
# Mutate each actual graph boundary without compiling or running target code.
for mutation in flat objects whole dependencies flags; do
    case "$mutation" in
        flat) expression="s/executable('test_runner',/executable('test_runner', test_runner_sources,/" ;;
        objects) expression="s/executable('test_runner',/executable('test_runner', objects: test_runner_cases.extract_all_objects(),/" ;;
        whole) expression='s/link_whole: test_runner_cases/link_with: test_runner_cases/' ;;
        dependencies) expression="/static_library('test_runner_cases'/,/build_by_default:/s/dependencies: test_runner_deps/dependencies: []/" ;;
        flags) expression="/static_library('test_runner_cases'/,/build_by_default:/s/-DSIXEL_PIXBUF_LOADER_TESTING/-DWRONG_CASE_FLAGS/" ;;
    esac
    sed "$expression" "$src_root/tests/meson.build" | awk -f "$checker" >&2 && {
        echo 'not ok 1 - Meson test runner archive mutation escaped detection'
        exit 0
    }
done
sh "$src_root/tests/_static/sh/check-test-runner-link.sh" "$src_root" >&2 || {
    echo 'not ok 1 - test runner object inventory and bounded link recipes'
    exit 0
}
echo 'ok 1 - test runner object inventory and bounded link recipes'
exit 0
