# Check the local workflow shape, not arbitrary YAML. Each attempt must make
# build failures data before the VM action decides whether copyback succeeded.
function valid(text,    blocks, n, b, tool, attempt, id, file, marker, j,
    captures, checks, start, finish, body, line, lines, k) {
    n = split(text, blocks, "\n    - name:")
    for (b = 1; b <= n; ++b) {
        body = blocks[b]
        if (body !~ /DragonFlyBSD/) continue
        tool = body ~ /Autotools/ ? "autotools" : "meson"
        if (body ~ /id: dragonflybsd_/) {
            line = body
            sub(/.*id: dragonflybsd_/, "", line)
            sub(/\n.*/, "", line)
            split(line, parts, "_")
            attempt = parts[2] + 0
            if (parts[1] != tool || attempt < 1 || attempt > 3) return 0
            id = "dragonflybsd_" tool "_" attempt
            if (seen[id]++) return 0
            file = "dragonflybsd-" tool "-" attempt ".result.log"
            marker = "sh tools/ci-vm-result.sh capture " file " <<'SIXEL_VM_BUILD'"
            start = index(body, marker)
            finish = index(body, "\n          SIXEL_VM_BUILD")
            if (!start || finish <= start || body ~ /prepare:/ ||
                !index(body, "continue-on-error: true") ||
                !index(body, "uses: vmactions/dragonflybsd-vm@v1")) return 0
            line = substr(body, finish + length("\n          SIXEL_VM_BUILD"))
            if (line !~ /^[ \n]*$/) return 0
            line = substr(body, start, finish - start)
            if (!index(line, "pkg install -y") ||
                !(tool == "autotools" ? index(line, "gmake V=1") &&
                  index(line, "}/configure") && index(line, "'check'") : \
                  index(line, "meson setup builddir") &&
                  index(line, "meson compile -C builddir") &&
                  index(line, "'test'"))) return 0
            for (j = 1; j < attempt; ++j) {
                if (!index(body, "steps.dragonflybsd_" tool "_" j ".outcome != 'success'")) return 0
            }
            ++captures
        } else if (body ~ /Validate DragonFlyBSD retry result/) {
            if (body ~ /outcome !=/ ||
                !index(body, "run: sh tools/ci-vm-result.sh check \"$RESULT_FILE\"")) return 0
            for (j = 1; j <= 3; ++j) {
                marker = "steps.dragonflybsd_" tool "_" j ".outcome == 'success' && 'dragonflybsd-" tool "-" j ".result.log'"
                if (!index(body, marker)) return 0
            }
            ++checks
        }
    }
    # POSIX awk has no whole-array delete.
    for (k in seen) delete seen[k]
    return captures == 6 && checks == 2
}
{ source = source $0 "\n" }
END {
    if (!valid(source)) {
        print "DragonFlyBSD must capture every build result before host validation"
        exit 1
    }
    # Removal controls must fail even when the other five attempts are valid.
    for (mutation = 1; mutation <= 4; ++mutation) {
        changed = source
        if (mutation == 1) sub(/capture dragonflybsd-autotools-2/, "missing dragonflybsd-autotools-2", changed)
        if (mutation == 2) sub(/check "\$RESULT_FILE"/, "true", changed)
        if (mutation == 3) gsub(/steps.dragonflybsd_autotools_1.outcome != 'success'/, "true", changed)
        if (mutation == 4) sub(/dragonflybsd-meson-3.result.log <<'SIXEL_VM_BUILD'/, "dragonflybsd-meson-1.result.log <<'SIXEL_VM_BUILD'", changed)
        for (key in seen) delete seen[key]
        if (changed == source || valid(changed)) {
            print "VM result handoff removal control escaped: " mutation
            exit 1
        }
    }
}
