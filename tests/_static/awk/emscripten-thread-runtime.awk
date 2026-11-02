# Protect executable-only Emscripten thread lifecycle flags in both builders.
function valid(text, path,    block, lines, n, i, line, count, name, active,
    depth, opens, closes, probe, seen) {
    if (path ~ /configure[.]ac$/) {
        n = split(text, lines, "SIXEL_PROGRAM_LDFLAGS=\"\"")
        if (n != 2) return 0
        block = lines[2]
        sub(/AC_SUBST\(\[SIXEL_PROGRAM_LDFLAGS\]\).*/, "", block)
        return index(block, "AS_IF([test \"x$have_threads\" = \"xyes\"]") &&
            index(block, "AS_CASE([$host_os], [emscripten*]") &&
            index(block, "-sPROXY_TO_PTHREAD=1 -sEXIT_RUNTIME=1") &&
            index(text, "AC_SUBST([SIXEL_PROGRAM_LDFLAGS])")
    }
    if (path ~ /\/meson[.]build$/ && path !~ /\/(tests|assessment|converters)\/meson[.]build$/) {
        n = split(text, lines, "program_link_args = \\[\\]")
        if (n != 2) return 0
        block = lines[2]
        sub(/endif.*/, "", block)
        return index(block, "if host_system == 'emscripten' and have_threads") &&
            index(block, "'-sPROXY_TO_PTHREAD=1', '-sEXIT_RUNTIME=1'")
    }
    gsub(/\\\n[ \t]*/, " ", text)
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        line = lines[i]
        if (path ~ /Makefile[.]am$/) {
            if (line ~ /^(img2sixel_LDFLAGS|sixel2png_LDFLAGS|lsqa_LDFLAGS|TESTS_COMMON_LDFLAGS_VAL) *=/) {
                ++count
                if (!index(line, "@SIXEL_PROGRAM_LDFLAGS@")) return 0
            } else if (index(line, "@SIXEL_PROGRAM_LDFLAGS@")) {
                return 0
            }
        } else {
            if (line ~ /executable\('(img2sixel|sixel2png|lsqa|test_runner)'/) {
                active = 1; seen = 0; depth = 0; ++count
            }
            if (!active) continue
            if (line ~ /link_args: *program_link_args,/) seen = 1
            probe = line
            gsub(/'[^']*'/, "", probe)
            opens = gsub(/\(/, "", probe)
            closes = gsub(/\)/, "", probe)
            depth += opens - closes
            if (!depth) {
                if (!seen) return 0
                active = 0
            }
        }
    }
    return count == (path ~ /\/converters\/(Makefile[.]am|meson[.]build)$/ ? 4 : \
        path ~ /\/assessment\/Makefile[.]am$/ ? 2 : 1) && !active
}
FNR == 1 {
    source[FILENAME] = ""
    collecting = 0
    kind = FILENAME ~ /configure[.]ac$/ ? "configure" : \
        FILENAME ~ /Makefile[.]am$/ ? "make" : \
        FILENAME !~ /\/(tests|assessment|converters)\/meson[.]build$/ \
        ? "meson" : "targets"
}
{
    line = $0
    sub(/#.*/, "", line)
    # Keep only the flag definition and linker assignments in large build
    # inputs; the C/source inventories are unrelated to this contract.
    if (kind == "configure") {
        if (line ~ /^SIXEL_PROGRAM_LDFLAGS=""/) collecting = 1
        if (!collecting) next
        if (line ~ /^AC_SUBST\(\[SIXEL_PROGRAM_LDFLAGS\]\)/) collecting = 0
    } else if (kind == "meson") {
        if (line ~ /^program_link_args = \[\]/) collecting = 1
        if (!collecting) next
        if (line ~ /^endif/) collecting = 0
    } else if (kind == "make") {
        if (line ~ /^(img2sixel_LDFLAGS|sixel2png_LDFLAGS|lsqa_LDFLAGS|TESTS_COMMON_LDFLAGS_VAL) *=/)
            collecting = 1
        if (!collecting && !index(line, "@SIXEL_PROGRAM_LDFLAGS@")) next
        collecting = line ~ /\\[ \t]*$/
    }
    source[FILENAME] = source[FILENAME] line "\n"
}
END {
    for (path in source) {
        ++files
        text = source[path]
        if (!valid(text, path)) {
            print path ": executable thread runtime flags missing or misplaced"
            failed = 1
        }
        # Delete each required flag, guard and consumer in turn. These
        # controls use the real source without creating fixture files.
        for (control = 1; control <= 3; ++control) {
            changed = text
            if (control == 1)
                edits = gsub(/-sPROXY_TO_PTHREAD=1/, "", changed)
            else if (control == 2)
                edits = gsub(/-sEXIT_RUNTIME=1/, "", changed)
            else if (path ~ /configure[.]ac$/)
                edits = gsub(/x\$have_threads/, "xwrong_condition", changed)
            else if (path ~ /\/meson[.]build$/ && path !~ /\/(tests|assessment|converters)\/meson[.]build$/)
                edits = gsub(/and have_threads/, "", changed)
            else
                edits = gsub(/@SIXEL_PROGRAM_LDFLAGS@|link_args: program_link_args,/, "", changed)
            if (edits && valid(changed, path)) {
                print path ": missing runtime setting escaped control " control
                failed = 1
            }
        }
    }
    if (files != 8) {
        print "expected all eight build inputs"
        failed = 1
    }
    exit failed
}
