# Lexical policy controls exercise the same scanner as repository sources.
function pattern_controls(    result, i) {
    control = 1
    control_case("test \"${x#*needle}\" != \"$x\"", 1, 1)
    control_case("{ test \"${x#*needle}\" != \"$x\"; }", 1, 2)
    control_case("(test \"${x#*needle}\" != \"$x\")", 1, 3)
    control_case("case \"$a\" in *) test \"${x#*needle}\" != \"$x\" ;; esac", 1, 4)
    control_case("test ! \"${x#*needle}\" != \"$x\"", 1, 5)
    control_case("while test \"${x#*needle}\" != \"$x\"; do :; done", 1, 6)
    control_case("test \"$x\" = \"${x%needle*}\"", 1, 7)
    control_case("[ \"${x##*needle}\" != \"${x}\" ]", 1, 8)
    control_case("test \"${x%%needle*}\" = \"$x\"", 1, 9)
    control_case("tail=${x#*needle}\ntest \"$tail\" != \"$x\"", 1, 10)
    control_case("tail=${x#*needle}\ncopy=\"$tail\"\ntest \"$copy\" != \"$x\"", 1, 11)
    control_case("test \"${x#*needle}\" \\\n != \"$x\"", 1, 12)
    control_case("test \"${x#'line\n'}\" != \"$x\"", 1, 13)
    control_case("test \"${x#*\"${needle}\"}\" != \"$x\"", 1, 14)
    control_case("[[ \"$x\" == *needle* ]]", 1, 15)
    control_case("[[ \"$x\" == $pattern ]]", 1, 16)
    control_case("[[ \"$x\" == \"${prefix}\"* ]]", 1, 17)
    control_case("[[ \"$x\" == \"*needle*\" ]]", 0, 18)
    control_case("[[ \"$x\" =~ needle ]]", 1, 19)
    control_case("case \"$x\" in *needle*) true ;; *) false ;; esac", 0, 20)
    control_case("tail=${trace#*marker*}\ncase \"$tail\" in *next*) :;; esac", 1, 21)
    control_case("tail=\"${trace#*\"${marker}\"*}\"", 1, 22)
    control_case("trace=${trace#*marker*}", 1, 23)
    control_case("test \"${trace#*marker*}\" != \"$trace\"", 1, 24)
    control_case("# tail=${trace#*marker*}", 0, 25)
    control_case("printf '%s' '${trace#*marker*}'", 0, 26)
    control_case("cat <<'EOF'\ntail=${trace#*marker*}\nEOF\n", 0, 27)
    control_case("tail=${trace#*marker\\*}", 0, 28)
    control_case("tail=${trace#*\"marker*\"}", 0, 29)
    control_case("tail=${trace#\"*marker*\"}", 0, 30)
    control_case("field=${trace#*key=}", 0, 31)
    control_case("field=${line##*=}\ntest \"$field\" = expected", 0, 32)
    control_case("test \"${file##*/}\" = fixture.png", 0, 33)
    control_case("x=${x#needle}\ntest \"$x\" = \"$x\"", 0, 34)
    control_case("# test \"${x#*needle}\" != \"$x\"", 0, 35)
    control_case("printf '%s\\n' 'test \"${x#*needle}\" != \"$x\"'", 0, 36)
    control_case("cat <<'EOF'\ntest \"${x#*needle}\" != \"$x\"\nEOF\n", 0, 37)
    control_case("cat <<'END OF DATA'\ntest \"${x#*needle}\" != \"$x\"\nEND OF DATA\n", 0, 38)
    control_case("cat <<- END-1\n\ttest \"${x#*needle}\" != \"$x\"\n\tEND-1\n", 0, 39)
    control_case("cat <<-EOF\n\ttest \"${x#*needle}\" != \"$x\"\n\tEOF\ntest \"${x#*needle}\" != \"$x\"", 1, 40)
    control_case("tail=${x#needle}\ntail=other\ntest \"$tail\" != \"$x\"", 0, 41)
    control = 0
    whitelist_controls()
}
function control_case(source, expected, id,    lines, n, i) {
    reset_pattern(); control_count = 0
    n = split(source, lines, "\n")
    for (i = 1; i <= n; i++) shell_line(lines[i] "\n", i)
    if (control_count != expected) {
        print "# pattern scanner control " id " expected " expected ", got " control_count
        failed = 1
    }
}
function whitelist_controls(    key) {
    key = "tests/control.t" SUBSEP 3 SUBSEP "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    finding[key] = 1
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 3, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\"}]}", 1) != 1) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 3, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"\", \"approval\": \"synthetic control, not an actual user approval\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 3, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/*.t\", \"line\": 3, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 0, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 3, \"sha256\": \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 4, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 3, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\", \"extra\": \"broad waiver\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": [{\"path\": \"tests/control.t\", \"line\": 3, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\"}, {\"path\": \"tests/control.t\", \"line\": 3, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", \"reason\": \"synthetic necessity control\", \"approval\": \"synthetic control, not an actual user approval\"}]}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 2, \"entries\": []}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    if (whitelist_document("{\"version\": 1, \"entries\": \"all\"}", 1) != 0) {
        print "# whitelist approval control failed"; failed = 1
    }
    delete finding[key]
    for (key in approved) delete approved[key]
}
