# Check case ownership using physical paths emitted by the shell walker.
function error(message) { print message; failed = 1 }
$1 == "L" { error($2 ": category cannot be a symlink"); next }
{
    path = $2; n = split(path, parts, "/"); name = parts[n]
    numbered = name ~ /^[0-9][0-9][0-9][0-9]_[a-zA-Z0-9][a-zA-Z0-9_-]*\.(t|sh|py|rb|pl|php|ps1)$/
    if (!numbered && name !~ /\.t$/) next
    if (!numbered) { error(path ": expected NNNN_observation.ext"); next }
    if (n != 3) { error(path ": expected two category levels"); next }
    if ($1 != "F") error(path ": case must be a regular source file")
    leaf = parts[1] "/" parts[2]
    number = substr(name, 1, 4) + 0
    cases[leaf]++
    numbers[leaf, number]++
    leaves[leaf] = parts[1]
}
END {
    for (leaf in leaves) {
        if (cases[leaf] > 200) error(leaf ": " cases[leaf] " cases exceed 200")
        for (i = 1; i <= cases[leaf]; i++) if (numbers[leaf, i] != 1) {
            error(leaf ": case numbers must be unique and contiguous from 0001"); break
        }
        domains[leaves[leaf]]++
    }
    for (domain in domains) {
        count++
        if (domains[domain] > 24) error(domain ": " domains[domain] " categories exceed 24")
    }
    if (count > 24) error("tests: " count " categories exceed 24")
    exit failed
}
