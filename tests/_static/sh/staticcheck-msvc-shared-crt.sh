#!/bin/sh
# DLL consumers must share the CRT that owns streams and allocated memory.
# Policy: docs/misc/platforms/msvc.md
# Coverage: MSVC-04
# Meson graph: docs/build/meson.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
# shellcheck disable=SC2016
awk '
function valid(text,    compact) {
    compact = text
    gsub(/[[:space:]]/, "", compact)
    return index(compact, "\047b_vscrt=md\047") &&
        index(compact, guard) && index(compact, probe)
}
function valid_configure(text,    compact) {
    compact = text
    gsub(/[[:space:]]/, "", compact)
    return index(compact, config_guard) &&
        index(compact, config_default) && index(compact, config_probe) &&
        index(compact, config_error)
}
BEGIN {
    guard = "if[\047msvc\047,\047clang-cl\047].contains(cc.get_id())and" \
        "get_option(\047default_library\047)!=\047static\047" \
        "ifnotcc.links(\047\047\047"
    probe = "#ifndef_DLL#errorAsharedlibsixelrequiresthesharedMSVCCRT" \
        "#endifintmain(void){return0;}\047\047\047," \
        "name:\047sharedMSVCCRTforDLLconsumers\047)" \
        "error(\047Sharedlibsixelrequiresb_vscrt=mdormdd" \
        "(matchingitsconsumers);usedefault_library=staticforastaticCRT\047)" \
        "endifendif"
    good = "\047b_vscrt=md\047" guard probe
    missing_default = good
    sub(/b_vscrt=md/, "b_vscrt=from_buildtype", missing_default)
    missing_clang = good
    sub(/,\047clang-cl\047/, "", missing_clang)
    compile_only = good
    sub(/cc.links/, "cc.compiles", compile_only)
    unchecked = good
    sub(/#ifndef_DLL/, "#ifdef_DLL", unchecked)
    advisory = good
    sub(/error\(/, "warning(", advisory)
    if (!valid(good) || valid(missing_default) || valid(missing_clang) ||
        valid(compile_only) || valid(unchecked) || valid(advisory)) {
        print "# MSVC CRT source controls failed"
        failed = 1
    }
    config_guard = "AS_IF([test\"x$using_cl\"=xyes&&" \
        "test\"x$enable_shared\"=xyes],["
    config_default = "AS_IF([test\"x$sixel_msvc_crt_selected\"=xno],[" \
        "CFLAGS=\"$CFLAGS-MD\"])"
    config_probe = "AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[" \
        "#ifndef_DLL#errorAsharedlibsixelrequiresthesharedMSVCCRT" \
        "#endif]],[[]])],[AC_MSG_RESULT([yes])],["
    config_error = "AC_MSG_ERROR([sharedlibsixelrequires-MDor-MDd" \
        "matchingitsconsumers;use--disable-sharedforastaticCRT])"
    config_good = config_guard config_default config_probe config_error
    config_bad = config_good
    sub(/#ifndef_DLL/, "#ifdef_DLL", config_bad)
    config_advisory = config_good
    sub(/AC_MSG_ERROR/, "AC_MSG_WARN", config_advisory)
    if (!valid_configure(config_good) || valid_configure(config_bad) ||
        valid_configure(config_advisory) ||
        valid_configure(config_guard config_probe config_error)) {
        print "# Autotools CRT source controls failed"
        failed = 1
    }
}
FILENAME == ARGV[1] { source = source $0 "\n" }
FILENAME == ARGV[2] {
    # Only retain the owned contiguous block, not the full configure input.
    if ($0 ~ /^AS_IF.*using_cl.*enable_shared/) in_crt = 1
    if (in_crt) configure = configure $0 "\n"
    if ($0 == "") in_crt = 0
}
END { exit failed || !valid(source) || !valid_configure(configure) }
' "$1/meson.build" "$1/configure.ac" || {
    echo 'not ok 1 - MSVC DLL builds require a shared CRT'
    exit 0
}
echo 'ok 1 - MSVC DLL builds require a shared CRT'
exit 0
