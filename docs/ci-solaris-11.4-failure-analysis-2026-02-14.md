# Solaris 11.4 CI failure analysis (2026-02-14)

Target jobs:

- `Autotools-solaris-11.4-x86_64`
- `Meson-solaris-11.4-x86_64`

Run analyzed: `22021279889` (commit `174db630184c980b53ee17dd47f694c14ff90ac2`).

## 1. Observed failures

### Common failures in both Autotools and Meson jobs

1. `codec/format/0003_filename_driven_png_header.t`
2. `processing/geometry/0002_dcs_crc_width_scaling.t`
3. `processing/geometry/0003_dcs_crc_height_scaling.t`
4. `processing/geometry/0004_dcs_crc_height_scaling_auto_width.t`
5. `processing/geometry/0005_dcs_crc_height_auto_width_absolute.t`
6. `processing/geometry/0006_dcs_crc_height_absolute_width_percent.t`
7. `io/loader/0028_loader_gnome_thumbnailer_exec_placeholders.t`
8. `io/loader/0034_loader_gnome_thumbnailer_unknown_placeholder_literal.t`
9. `io/loader/0038_loader_gnome_thumbnailer_exec_quoting_escape.t`
10. `io/loader/0040_loader_gnome_thumbnailer_tryexec_absolute_missing_skip.t`
11. `quant/mapfile/0004_mapfile_export_riff.t`
12. `planner/pipeline/0003_pipeline_planner_baseline_threads.t`
13. `planner/pipeline/0005_pipeline_planner_overrides_summary.t`
14. `planner/pipeline/0006_pipeline_planner_overrides_threads.t`
15. `cli/sixel2png/0005_stdin_default_output.t`

### Additional failures in Autotools only

16. `docs/consistency/0001_help_vs_man.t`
17. `docs/consistency/0002_man_vs_bash_completion.t`
18. `io/loader/0032_loader_gnome_thumbnailer_hint_size_invalid_env.t`
19. `io/loader/0037_loader_gnome_thumbnailer_hint_size_zero_env.t`

## 2. Failure clustering and likely causes

### Cluster A: byte/header and checksum oriented tests

- `codec/format/0003_*`
- `processing/geometry/0002..0006_*`
- `quant/mapfile/0004_*`
- `cli/sixel2png/0005_*`

These tests extract hex output with `od` and compare fixed lowercase hex strings.
On Solaris, `od` output formatting differs from GNU coreutils assumptions in several edge cases.
Because expected values are strict string matches, formatting differences alone can cause false negatives.

Likely countermeasure:

- Normalize extracted hex with `tr 'A-F' 'a-f'` and strip non-hex characters before comparison.
- Avoid tool-specific `od` output assumptions by preferring a helper function in `tests/_lib/sh/common.sh`.

### Cluster B: gnome-thumbnailer parser behavior tests

- `io/loader/0028`, `0034`, `0038`, `0040` (and Autotools-only `0032`, `0037`)

These tests rely on shell environment propagation and exact parser behavior for
`Exec`/`TryExec` handling and placeholder expansion.
The failures are Solaris-specific and concentrated in freedesktop-thumbnailer scenarios,
suggesting platform-dependent tokenization/command execution behavior in loader code or shell command resolution.

Likely countermeasure:

- Add focused diagnostics in those tests (print generated thumbnailer command/log content on failure).
- Compare Solaris results against Linux baseline and then decide whether to:
  - make parser behavior consistent across platforms, or
  - relax tests to accept equivalent behavior where spec allows flexibility.

### Cluster C: pipeline planner verbosity parsing tests

- `planner/pipeline/0003`, `0005`, `0006`

These tests scrape `-v` output and assert specific line structures (thread split summary etc.).
Solaris logger output or scheduling-related values may differ enough to break current exact-match heuristics.

Likely countermeasure:

- Assert semantic markers only (e.g., presence of keys regardless of spacing/order).
- Add a small parser helper for key-value extraction to avoid brittle `grep|head` pattern dependencies.

### Cluster D: docs consistency tests (Autotools only)

- `docs/consistency/0001`, `0002`

Only Autotools failed, Meson did not. This usually indicates generation-time differences
in man/help/completion artifacts under the Autotools toolchain on Solaris.

Likely countermeasure:

- Reproduce on Solaris with `./configure && gmake check` and capture the exact diff output.
- Ensure option ordering source-of-truth is shared (single generator or single canonical option list)
  so man/help/completion cannot drift by buildsystem.

## 3. Recommended action plan

1. **Short-term (stabilize Solaris CI quickly)**
   - Harden hex/checksum tests with portable normalization helpers.
   - Relax planner output matching to semantic checks.

2. **Mid-term (root cause on loader/docs)**
   - Add failure diagnostics for thumbnailer and docs consistency tests.
   - Run targeted Solaris matrix subset for those categories until stable.

3. **Long-term (prevent recurrence)**
   - Introduce shared shell helper APIs in `tests/_lib/sh/common.sh` for:
     - hex extraction,
     - key-value log matching,
     - safer env-passing wrappers.
   - Add a CI lint pass that flags GNU-specific options in TAP scripts.

## 4. Reproduction commands used for investigation

```sh
gh run list --limit 30 --json databaseId,headSha,conclusion,createdAt

gh run view 22021279889 --json jobs \
  --jq '.jobs[] | select(.name=="Meson-solaris-11.4-x86_64" or .name=="Autotools-solaris-11.4-x86_64") | [.name,.databaseId,.conclusion] | @tsv'

gh run view --job 63631929830 --log > /tmp/autotools-solaris.log
rg -n "FAIL|thread split|manpage|thumbnailer|riff|stdin" /tmp/autotools-solaris.log

gh run view --job 63631929825 --log > /tmp/meson-solaris.log
rg -n " [0-9]+/[0-9]+ .* FAIL" /tmp/meson-solaris.log
```
