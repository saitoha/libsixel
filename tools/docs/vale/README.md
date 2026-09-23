# Advisory documentation lint implementation

[Documentation governance](../../../docs/AGENTS.md#advisory-documentation-set-checks) owns the purpose and inventory conventions. This directory contains the optional implementation, tested with Vale 3.22.0 and Python 3.10 or later (standard library only). It uses only the local `Libsixel` style: no packages, synchronization, network access, build integration or automatic fixes.

Run from the repository root:

```sh
tools/docs/lint-prose
tools/docs/lint-prose --inventory-only
tools/docs/lint-prose --json /path/to/findings.json
```

The wrapper also works from another working directory when invoked by its path. A JSON destination is relative to the caller and its parent must already exist. It contains Vale's full alerts, including rule, message, match, line and span; the console prints totals per rule and per document. If Vale is absent, the inventory check still runs. Missing Python, malformed inventory, Vale errors and findings are reported but never fail the wrapper. A skipped or incomplete run is not a clean result; JSON is written only after a successful Vale run, so an existing output file can be stale after a skip or failure.

## Configuration and scopes

[`vale.ini`](vale.ini) lives here to keep the prototype opt-in, like the optional site tooling, instead of implicitly enabling editor lint across the repository. `StylesPath` resolves to the sibling [`styles/`](styles/) directory. The wrapper derives a transient configuration from this base and [`doc-types.tsv`](../doc-types.tsv), makes the style path absolute, and passes it with `--config` and `--no-global`. The inventory is the single classification list; there is no second hand-maintained list of reference pages in the INI. Running Vale directly with only the base config gives terminology and timing checks, without the inventory-derived audience rules.

| Rule | Level | Effective scope |
| --- | --- | --- |
| `Terminology` | warning | Rendered prose in every `docs/**/*.md`, including top-level pages, testing and governance. |
| `ContributorPaths` | warning | `src/`, `tests/` and `tools/` paths in prose and visible link labels on `user` and `both` pages. |
| `CommitReferences` | warning | Commit URLs and `commit <hash>` on `user` and `both` pages. Raw scope deliberately inspects link destinations; code examples and comments can also match. |
| `ChangeLogPhrasing` | suggestion | Historical wording on `user` and `both` pages whose primary type is `reference`; link labels are excluded. |
| `LocalProtocol` | suggestion | Counted warm-ups, measured/timed runs and “median of N” outside `docs/quality/`, including contributor measurement guides. |

Audience rules and `LocalProtocol` exclude `docs/testing/**` and `docs/AGENTS.md`. Unclassified pages retain only the base terminology/protocol checks and generate inventory diagnostics. The inventory check compares exact path sets with `git ls-files`, checks columns and enum values, reports duplicates and requires a note for `both`. Empty notes use the quoted TSV field `""` to preserve four columns without trailing tabs. Stage an added or renamed document before expecting the tracked-file check to agree. Vale scans Markdown under `docs/`, including untracked drafts, while inventory completeness concerns tracked Markdown only.

Vale's [Markdown scopes](https://docs.vale.sh/topics/scopes) omit inline code, fenced code and link destinations from prose rules. `TokenIgnores` additionally protects bare long options such as `--working-colorspace=linear`; the terminology regex protects `colorspace` in path components and filenames, including visible link labels. Only the commit rule inspects raw destinations. These boundaries are intentional: a path inside inline code will not contribute to the prose-path count, and counts are not a complete repository-link census. The prototype neither checks policy ownership nor proves that a local timing description is normative or contradictory.

## Terminology choices and limits

These are candidate editorial preferences, not a new API vocabulary or an instruction to replace every match. The initial whole-word, case-insensitive raw-source counts below include code and link targets; lint counts use rendered prose instead.

| Concept | Preferred prose term | Evidence and limits |
| --- | --- | --- |
| Color coordinate system | **color space** | `colorspace` occurs in 55 documents, `color space` in 27 and `color-space` in 18. The [concept owner](../../../docs/concepts/colorspace.md) uses “Color Spaces” and the policy titles use “Color Space”; CLI `--working-colorspace`, `--clustering-colorspace` and `SIXEL_COLORSPACE_*` retain their exact spellings. The case-sensitive metadata name `ColorSpace` is preserved. |
| Configured concurrency allowance | **worker budget** | `worker budget` occurs in 12 documents, `thread budget` in 9, `thread count` in 14 and `worker count` in 6. The [threading owner](../../../docs/threading/overview.md#worker-budget-is-not-a-process-thread-limit) distinguishes the `--threads` allowance from active workers and total OS threads. Only budget variants and explicitly configured/requested counts are candidates; unqualified thread/worker counts remain valid measurements. |
| Requested palette capacity | **palette size** | `palette size` occurs in 20 documents, `color count` in 8, `palette budget` in 4 and `number of colors` in 3. The [quantization reference](../../../docs/functionality/quantization.md#cli-surface) calls `-p COLORS` the requested palette size; `SIXEL_OPTFLAG_COLORS` and CLI help still say “number of colors”. The rule narrows color-count/number-of-colors substitutions to requested or explicitly palette-qualified quantities. Distinct image colors and actual used colors are different quantities. |
| In-memory palette slot | **palette entry** | `palette entry` occurs in 19 documents, `palette register` in 5 and `color register` in 5. The CLI and [palette application model](../../../docs/functionality/encoding-pipeline.md#palette-application) use entries. The register alternatives are measured as review candidates, but **color register remains correct for mutable SIXEL wire/terminal state**, as defined in the [format reference](../../../docs/sixel-format.md#graphics-color-introducer). This lexical rule cannot infer that distinction and must not become blocking or drive bulk replacements. |

The rule also recognizes plurals. Hyphenated attributive “color-space” can be legitimate English, and historical source quotations can require their original terms. Review findings in context. Likewise, a contributor path can be a useful evidence pointer, a commit can establish attribution, and a timed-run count can describe an experiment rather than impose a shared protocol.
