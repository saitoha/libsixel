# The runner's large case inventory belongs to a whole-linked static archive.
function value(body, key,    result) {
    if (!match(body, key ":[[:space:]]*[^,\n]+")) return ""
    result = substr(body, RSTART, RLENGTH)
    sub(/^[^:]*:[[:space:]]*/, "", result)
    gsub(/[[:space:]]/, "", result)
    return result
}
function flags(body,    result) {
    result = body
    if (!sub(/^.*c_args:[[:space:]]*/, "", result)) return ""
    if (!sub(/,[[:space:]]*build_by_default:.*/, "", result)) return ""
    gsub(/[[:space:]]/, "", result)
    return result
}
{
    sub(/#.*/, "")
    if ($0 ~ /=[[:space:]]*static_library\('test_runner_cases'/) {
        active = 1
        archive_name = $1
    }
    if ($0 ~ /executable\('test_runner'/) active = 2
    if (active == 1) archive = archive $0 "\n"
    if (active == 2) runner = runner $0 "\n"
    if (active && $0 ~ /\)[[:space:]]*$/) active = 0
}
END {
    if (archive == "" || runner == "" ||
        archive !~ /test_runner_sources/ || runner ~ /test_runner_sources/ ||
        value(runner, "link_whole") != archive_name || runner ~ (archive_name "[[:space:]]*\\.extract") ||
        value(archive, "include_directories") == "" ||
        value(archive, "include_directories") != value(runner, "include_directories") ||
        value(archive, "dependencies") == "" ||
        value(archive, "dependencies") != value(runner, "dependencies") ||
        flags(archive) == "" || flags(archive) != flags(runner)) {
        print "Meson runner must whole-link its case archive with matching compile inputs"
        exit 1
    }
}
