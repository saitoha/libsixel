# Verify generated source metadata without requiring the authoring tool's Python.
# Prose outside generated tables remains the documentation owner's responsibility.
function problem(message) { print "# " message; failed = 1 }
function trim(value) { sub(/^[[:space:]]+/, "", value); sub(/[[:space:]]+$/, "", value); return value }
function normalize(value) {
    sub(/^[Tt][Aa][Pp] [Tt][Ee][Ss][Tt]:[[:space:]]*/, "", value)
    gsub(/[[:space:]]+/, " ", value); value = trim(value)
    gsub(/\|/, "\\|", value)
    if (length(value) > 280) value = trim(substr(value, 1, 277)) "..."
    if (value != "" && substr(value, length(value)) !~ /[.!?)`\]]/) value = value "."
    return value
}
function family(path,    stem, name, i) {
    name = path; sub(/^.*\//, "", name)
    if (name == "0001_lsqa_format_grayscale.t") return 5
    if (name == "0001_png_palette_samples.t" || name == "0001_lsqa_format_rgb.t") return 4
    stem = tolower(path); sub(/^tests\//, "", stem); sub(/\.[^.]+$/, "", stem)
    gsub(/[-\/]/, "_", stem)
    for (i = 1; i <= 10; i++) if (stem ~ "(^|_)(" tokens[order[i]] ")(_|$)") return order[i]
    return 1
}
function assurance(path, source,    stem) {
    stem = tolower(path); sub(/^.*\//, "", stem); sub(/\.[^.]+$/, "", stem)
    source = tolower(source)
    if (source ~ /loader\/image_(display_values|values|sample_range|dimensions)([^A-Za-z0-9_]|$)/) return 1
    if (source ~ /lsqa_path|ms-ssim/ || stem ~ /msssim|_lsqa/) return 2
    if (stem ~ /(^|_)(numeric|digest)(_|$)/) return 1
    if (stem ~ /(^|_)(trace|code|status)(_|$)/) return 3
    if (stem ~ /(^|_)(reject|rejects|invalid|corrupt|truncated|bad|fail)(_|$)/) return 4
    if (stem ~ /(^|_)(watchdog|sigint|oom|allocation|resource|overflow)(_|$)/) return 5
    return 6
}
function record(path,    file, lines, source, line, n, status, policies, seen, i, text, observation, started, label, quote, f, a, context, row, backlink) {
    file = root "/" path
    while ((status = (getline line < file)) > 0) {
        lines[++n] = line; source = source line "\n"
        if (line == "# Test-plan: docs/testing/builtin-loader-coverage.md") {
            backlink++; if (n > 20) problem(path ": late Test-plan backlink")
        }
    }
    close(file)
    if (status < 0 || n == 0) { problem("cannot read " path); return }
    if (backlink != 1) problem(path ": expected exactly one Test-plan backlink")
    for (i = 1; i <= n && i <= 20; i++) if (lines[i] ~ /^# Policy: docs\/[A-Za-z0-9_.\/-]+\.md$/) {
        text = substr(lines[i], 11)
        if (!(text in seen)) {
            seen[text] = 1
            policies = policies (policies == "" ? "" : ", ") "[`" text "`](../" substr(text, 6) ")"
        }
    }
    for (i = 2; i <= n && i <= 20; i++) {
        line = lines[i]
        if (line == "" || substr(line, 1, 1) != "#") { if (started) break; else continue }
        text = trim(substr(line, 2))
        if (text ~ /^(Policy:|Test-plan:|Staticcheck:|shellcheck)/) continue
        if (text ~ /^(Fixture generation|Fixture\/expected|Reference generation|Reproduction command|Regeneration command|python3 |magick |convert )/) break
        if (text == "") { if (started) break; else continue }
        observation = observation (started ? " " : "") text; started = 1
    }
    if (observation == "") for (i = 1; i <= n && i <= 40; i++) {
        line = lines[i]
        if (line !~ /^[A-Z0-9_]*LABEL=["\047]/) continue
        sub(/^[^=]*=/, "", line); quote = substr(line, 1, 1)
        if (substr(line, length(line)) != quote) continue
        observation = substr(line, 2, length(line) - 2); break
    }
    if (observation == "") {
        observation = path; sub(/^.*\//, "", observation); sub(/\.[^.]+$/, "", observation)
        sub(/^[0-9]+_/, "", observation); gsub(/_/, " ", observation)
        observation = toupper(substr(observation, 1, 1)) substr(observation, 2)
    }
    observation = normalize(observation)
    f = family(path); a = assurance(path, source)
    counts[f, a]++; totals[a]++; sizes[f]++; total++
    context = policies == "" ? "Inventory only" : "Policy-linked: " policies
    row = "| [" path "](../../" path ") | " observation " | " kinds[a] " | " context " |"
    rows[f, sizes[f]] = row
}
BEGIN {
    split("shared netpbm gif png jpeg hdr psd bmp sixel tga pic", keys, " ")
    split("7 9 6 8 4 3 5 11 10 2", order, " ")
    tokens[2] = "pnm|pam|ppm|pgm|pbm|netpbm"; tokens[3] = "gif"
    tokens[4] = "png|apng"; tokens[5] = "jpeg|jpg"; tokens[6] = "hdr"
    tokens[7] = "psd|psb"; tokens[8] = "bmp|dib"; tokens[9] = "sixel"; tokens[10] = "tga"; tokens[11] = "pic"
    split("Shared loader integration and policy;Netpbm;GIF;PNG and APNG;JPEG;Radiance HDR;PSD and PSB;BMP and DIB;SIXEL;TGA;Softimage PIC", titles, ";")
    split("Direct numeric/digest;Perceptual quality threshold;Trace/status observation;Acceptance/rejection;Robustness/liveness;Other integration/smoke", kinds, ";")
    split("Builtin Image Loader;Netpbm builtin component;GIF builtin component;PNG/APNG builtin component;JPEG builtin component;Radiance HDR builtin component;PSD/PSB builtin component;BMP builtin component;SIXEL builtin component;TGA builtin component;Softimage PIC builtin component", labels, ";")
    for (i = 1; i <= 11; i++) documents[i] = "../loader/builtin/" keys[i] ".md"
    documents[1] = "../loader/builtin.md"; documents[4] = "../loader/builtin/png/README.md"
    manifest = root "/tests/_support/test-plans/builtin-loader.list"
    while ((status = (getline path < manifest)) > 0) record(path)
    close(manifest)
    if (status < 0 || total == 0) problem("cannot read builtin-loader manifest")
    for (i = 1; i <= 11; i++) {
        row = "| [" titles[i] "](" documents[i] ") | " (sizes[i] + 0)
        for (j = 1; j <= 6; j++) row = row " | " (counts[i, j] + 0)
        summary[++summary_count] = row " |"
        if (!sizes[i]) continue
        inventory[++inventory_count] = "### " titles[i]
        inventory[++inventory_count] = "Primary implementation context: [" labels[i] "](" documents[i] ")."
        inventory[++inventory_count] = "| Test | Observation | Assertion mechanism | Policy context |"
        inventory[++inventory_count] = "| --- | --- | --- | --- |"
        for (j = 1; j <= sizes[i]; j++) inventory[++inventory_count] = rows[i, j]
    }
    row = "| **Total** | **" total "**"
    for (j = 1; j <= 6; j++) row = row " | **" (totals[j] + 0) "**"
    summary[++summary_count] = row " |"
}
/^<!-- test-plan: @tests\/_support\/test-plans\/builtin-loader.list -->$/ { inside = 1; next }
/^<!-- test-plan-end -->$/ { inside = 0; next }
inside && NF {
    if ($0 != inventory[++inventory_read]) problem(FILENAME ":" FNR ": stale inventory metadata: " $0)
    next
}
/^\| \[.*\]\(\.\.\/loader\/builtin/ || /^\| \*\*Total\*\*/ {
    if ($0 != summary[++summary_read]) problem(FILENAME ":" FNR ": stale inventory summary")
}
END {
    if (inventory_read != inventory_count || summary_read != summary_count) problem("incomplete generated builtin-loader tables")
    exit failed
}
