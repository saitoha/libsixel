#!/usr/bin/env python3
"""Generate the exhaustive builtin-loader shell-test inventory."""

from __future__ import annotations

import argparse
import difflib
import re
import sys
from dataclasses import dataclass
from pathlib import Path


PLAN_DOCUMENT = Path("docs/testing/builtin-loader-coverage.md")
TEST_DIRECTORY = Path("tests/loader/builtin")
TEST_PLAN = "docs/testing/builtin-loader-coverage.md"
TEST_PLAN_COMMENT = f"# Test-plan: {TEST_PLAN}"
TEST_PLAN_GLOB = "tests/loader/builtin/*.t"

ASSURANCE_KINDS = (
    "Direct numeric/digest",
    "Perceptual quality threshold",
    "Trace/status observation",
    "Acceptance/rejection",
    "Robustness/liveness",
    "Other integration/smoke",
)


@dataclass(frozen=True)
class Family:
    """One documentation section and its filename classification tokens."""

    key: str
    title: str
    document: str
    document_label: str
    tokens: tuple[str, ...]


FAMILIES = (
    Family(
        "shared",
        "Shared loader integration and policy",
        "docs/loader/builtin.md",
        "Builtin Image Loader",
        (),
    ),
    Family(
        "netpbm",
        "Netpbm",
        "docs/loader/builtin/netpbm.md",
        "Netpbm builtin component",
        ("pnm", "pam", "ppm", "pgm", "pbm", "netpbm"),
    ),
    Family(
        "gif",
        "GIF",
        "docs/loader/builtin/gif.md",
        "GIF builtin component",
        ("gif",),
    ),
    Family(
        "png",
        "PNG and APNG",
        "docs/loader/builtin/png.md",
        "PNG/APNG builtin component",
        ("png", "apng"),
    ),
    Family(
        "jpeg",
        "JPEG",
        "docs/loader/builtin/jpeg.md",
        "JPEG builtin component",
        ("jpeg", "jpg"),
    ),
    Family(
        "hdr",
        "Radiance HDR",
        "docs/loader/builtin/hdr.md",
        "Radiance HDR builtin component",
        ("hdr",),
    ),
    Family(
        "psd",
        "PSD and PSB",
        "docs/loader/builtin/psd.md",
        "PSD/PSB builtin component",
        ("psd", "psb"),
    ),
    Family(
        "bmp",
        "BMP and DIB",
        "docs/loader/builtin/bmp.md",
        "BMP builtin component",
        ("bmp", "dib"),
    ),
    Family(
        "webp",
        "WebP",
        "docs/loader/builtin/webp.md",
        "WebP builtin component",
        ("webp",),
    ),
    Family(
        "sixel",
        "SIXEL",
        "docs/loader/builtin/sixel.md",
        "SIXEL builtin component",
        ("sixel",),
    ),
    Family(
        "tga",
        "TGA",
        "docs/loader/builtin/tga.md",
        "TGA builtin component",
        ("tga",),
    ),
    Family(
        "pic",
        "Softimage PIC",
        "docs/loader/builtin/pic.md",
        "Softimage PIC builtin component",
        ("pic",),
    ),
)


# More specific families precede reference formats such as PNM. A test named
# "psd_..._reference_pnm" therefore stays with the PSD decoder it exercises.
CLASSIFICATION_ORDER = (
    "psd",
    "webp",
    "sixel",
    "hdr",
    "bmp",
    "png",
    "gif",
    "jpeg",
    "pic",
    "tga",
    "netpbm",
)


# These early quality tests predate the format token naming convention. Keep
# their primary decoder ownership explicit instead of inferring it from prose.
CLASSIFICATION_OVERRIDES = {
    "0001_lsqa_format_grayscale.t": "jpeg",
    "0002_lsqa_format_palette.t": "png",
    "0003_lsqa_format_rgb.t": "png",
}


def source_root() -> Path:
    """Return the repository root from this script's checked-in location."""

    return Path(__file__).resolve().parent.parent


def family_map() -> dict[str, Family]:
    """Return families by stable internal key."""

    return {family.key: family for family in FAMILIES}


def has_token(stem: str, tokens: tuple[str, ...]) -> bool:
    """Match filename tokens without confusing references or substrings."""

    alternatives = "|".join(re.escape(token) for token in tokens)
    return re.search(rf"(?:^|_)(?:{alternatives})(?:_|$)", stem) is not None


def classify(path: Path) -> str:
    """Classify a test by the primary format named in its filename."""

    if path.name in CLASSIFICATION_OVERRIDES:
        return CLASSIFICATION_OVERRIDES[path.name]
    stem = path.stem.lower()
    families = family_map()
    for key in CLASSIFICATION_ORDER:
        if has_token(stem, families[key].tokens):
            return key
    return "shared"


def policy_references(lines: list[str]) -> tuple[str, ...]:
    """Read public-contract owners from the source-comment header."""

    references = []
    for line in lines[:20]:
        match = re.match(r"^# Policy: (docs/[A-Za-z0-9_./-]+\.md)$", line)
        if match is not None:
            references.append(match.group(1))
    return tuple(dict.fromkeys(references))


def normalize_observation(text: str) -> str:
    """Normalize source prose for a compact Markdown table cell."""

    text = re.sub(r"^TAP test:\s*", "", text, flags=re.IGNORECASE)
    text = re.sub(r"\s+", " ", text).strip()
    text = text.replace("|", "\\|")
    if len(text) > 280:
        text = text[:277].rstrip() + "..."
    if text and text[-1] not in ".!?)`]":
        text += "."
    return text


def opening_observation(lines: list[str]) -> str | None:
    """Extract the first explanatory comment block before test setup."""

    comments = []
    started = False
    for line in lines[1:20]:
        if line == "":
            if started:
                break
            continue
        if not line.startswith("#"):
            if started:
                break
            continue
        text = line[1:].strip()
        if text.startswith(("Policy:", "Test-plan:", "shellcheck")):
            continue
        if text.startswith(
            (
                "Fixture generation",
                "Fixture/expected",
                "Reference generation",
                "Reproduction command",
                "Regeneration command",
            )
        ):
            break
        if text.startswith(("python3 ", "magick ", "convert ")):
            break
        if not text:
            if started:
                break
            continue
        comments.append(text)
        started = True
    if not comments:
        return None
    return normalize_observation(" ".join(comments))


def case_label_observation(lines: list[str]) -> str | None:
    """Use generated matrix labels when wrappers have no prose header."""

    pattern = re.compile(
        r"^[A-Z0-9_]*(?:CASE_)?LABEL=(['\"])(.*)\1$"
    )
    for line in lines[:40]:
        match = pattern.match(line)
        if match is not None:
            return normalize_observation(match.group(2))
    return None


def filename_observation(path: Path) -> str:
    """Provide a deterministic fallback when a test lacks header prose."""

    stem = re.sub(r"^[0-9]+_", "", path.stem)
    text = stem.replace("_", " ")
    return normalize_observation(text[0].upper() + text[1:])


def observation(path: Path, lines: list[str]) -> str:
    """Return the best source-backed one-line test observation."""

    return (
        opening_observation(lines)
        or case_label_observation(lines)
        or filename_observation(path)
    )


def document_link(document: str) -> str:
    """Resolve a docs-relative policy path from docs/testing/."""

    return "../" + document.removeprefix("docs/")


def assurance_kind(path: Path, lines: list[str]) -> str:
    """Classify the assertion mechanism without inferring completeness."""

    name = path.stem.lower()
    source = "\n".join(lines).lower()
    if ("lsqa_path" in source or "ms-ssim" in source or
            "msssim" in name or "_lsqa" in name):
        return "Perceptual quality threshold"
    if re.search(r"(?:^|_)(?:numeric|digest)(?:_|$)", name):
        return "Direct numeric/digest"
    if re.search(r"(?:^|_)(?:trace|code|status)(?:_|$)", name):
        return "Trace/status observation"
    if re.search(
        r"(?:^|_)(?:reject|rejects|invalid|corrupt|truncated|bad|fail)"
        r"(?:_|$)",
        name,
    ):
        return "Acceptance/rejection"
    if re.search(
        r"(?:^|_)(?:watchdog|sigint|oom|allocation|resource|overflow)"
        r"(?:_|$)",
        name,
    ):
        return "Robustness/liveness"
    return "Other integration/smoke"


def policy_context(policies: tuple[str, ...]) -> str:
    """Render policy traceability independently of assertion strength."""

    if not policies:
        return "Inventory only"
    links = ", ".join(
        f"[`{policy}`]({document_link(policy)})" for policy in policies
    )
    return f"Policy-linked: {links}"


def read_tests(root: Path) -> list[dict[str, object]]:
    """Read and classify every nonrecursive builtin-loader shell test."""

    tests = []
    for path in sorted((root / TEST_DIRECTORY).glob("*.t")):
        relative = path.relative_to(root)
        lines = path.read_text(encoding="utf-8").splitlines()
        policies = policy_references(lines)
        tests.append(
            {
                "path": relative,
                "family": classify(relative),
                "observation": observation(relative, lines),
                "policies": policies,
                "assurance": assurance_kind(relative, lines),
            }
        )
    if not tests:
        raise RuntimeError(f"no tests matched {TEST_DIRECTORY}/*.t")
    return tests


def render_summary(tests: list[dict[str, object]]) -> list[str]:
    """Render family counts without pretending counts prove coverage."""

    lines = [
        "| Area | Tests | Direct values | Quality floor | Trace/status "
        "| Accept/reject | Robustness | Other/smoke |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for family in FAMILIES:
        members = [test for test in tests if test["family"] == family.key]
        counts = {
            kind: sum(test["assurance"] == kind for test in members)
            for kind in ASSURANCE_KINDS
        }
        lines.append(
            f"| [{family.title}]({document_link(family.document)}) "
            f"| {len(members)} "
            f"| {counts['Direct numeric/digest']} "
            f"| {counts['Perceptual quality threshold']} "
            f"| {counts['Trace/status observation']} "
            f"| {counts['Acceptance/rejection']} "
            f"| {counts['Robustness/liveness']} "
            f"| {counts['Other integration/smoke']} |"
        )
    counts = {
        kind: sum(test["assurance"] == kind for test in tests)
        for kind in ASSURANCE_KINDS
    }
    lines.append(
        f"| **Total** | **{len(tests)}** "
        f"| **{counts['Direct numeric/digest']}** "
        f"| **{counts['Perceptual quality threshold']}** "
        f"| **{counts['Trace/status observation']}** "
        f"| **{counts['Acceptance/rejection']}** "
        f"| **{counts['Robustness/liveness']}** "
        f"| **{counts['Other integration/smoke']}** |"
    )
    return lines


def render_inventory(tests: list[dict[str, object]]) -> list[str]:
    """Render every test exactly once inside the enforced marker pair."""

    lines = [f"<!-- test-plan: {TEST_PLAN_GLOB} -->", ""]
    for family in FAMILIES:
        members = [test for test in tests if test["family"] == family.key]
        lines.extend(
            [
                f"### {family.title}",
                "",
                f"Primary implementation context: "
                f"[{family.document_label}]({document_link(family.document)}).",
                "",
                "| Test | Observation | Assertion mechanism | Policy context |",
                "| --- | --- | --- | --- |",
            ]
        )
        for test in members:
            relative = test["path"].as_posix()
            target = "../../" + relative
            context = policy_context(test["policies"])
            lines.append(
                f"| [{relative}]({target}) | {test['observation']} "
                f"| {test['assurance']} | {context} |"
            )
        lines.append("")
    lines.append("<!-- test-plan-end -->")
    return lines


def render_document(tests: list[dict[str, object]]) -> str:
    """Render the complete durable inventory document."""

    lines = [
        "# Builtin Loader Coverage Inventory",
        "",
        "## Purpose",
        "",
        "This inventory keeps every shell test in `tests/loader/builtin/` discoverable without turning every generated matrix cell, malformed-input probe, or quality smoke test into a public compatibility promise. The format contracts and implementation maps remain owned by the [builtin format component documentation](../loader/builtin/README.md). A `Policy:` backlink records traceability, while the separately generated assertion mechanism records what the test actually observes; a policy link does not turn an LSQA threshold into an exact decoded-value assertion.",
        "",
        "The observation column is generated from the opening explanatory comment, then from a matrix `CASE_LABEL` when present, and finally from the test filename. Assertion mechanisms are classified conservatively from explicit test vocabulary: LSQA/MS-SSIM use is a perceptual quality threshold, `_numeric` and `_digest` cases directly compare decoded values, `_trace` and `_code` cases observe protocol or status evidence, and remaining names fall into acceptance/rejection, robustness/liveness, or other integration/smoke buckets. The test itself remains authoritative. Filename tokens assign each test to one primary format section; cross-format reference images such as PNM baselines do not create duplicate inventory rows.",
        "",
        "## Coverage summary",
        "",
        "Counts are rebuilt from the suite and show assertion mechanisms, not semantic completeness. `Perceptual quality threshold` means a minimum regression floor: it neither fixes exact decoded pixels nor proves metadata, branch selection, or error behavior. `Direct numeric/digest` fixes decoded values but does not by itself prove that an independently implemented decoder would produce the same values. A large generated matrix may still exercise many combinations of one branch while leaving another branch untested.",
        "",
        *render_summary(tests),
        "",
        "## Complete inventory",
        "",
        *render_inventory(tests),
        "",
        "## Related SIXEL suites",
        "",
        "The builtin SIXEL adapter delegates to the reusable decoder, so its broader parser, paint, parallelism, and output-format coverage lives in [`tests/processing/decoder`](../../tests/processing/decoder), while command-line decode coverage lives in [`tests/cli/sixel2png`](../../tests/cli/sixel2png). Those directories are not absorbed into this inventory because they also own decoder APIs and command behavior outside `tests/loader/builtin/`; representative adapter and decoder contracts remain linked from the [SIXEL builtin component](../loader/builtin/sixel.md).",
        "",
        "## Coverage boundary",
        "",
        "This inventory proves discoverability and exact suite membership. It does not prove that every format dialect, bit depth, compression mode, metadata combination, error boundary, or loader-policy interaction has a test. The coverage-audit notes in the individual format documents identify known semantic gaps, and review must compare those claims with the implementation maps rather than infer completeness from test counts.",
        "",
        "## Maintenance",
        "",
        "The marker glob, every link in the delimited inventory, and every reciprocal `Test-plan:` source comment are compared by [`staticcheck-test-plan-links`](../../tests/_static/sh/staticcheck-test-plan-links.sh). Additions, deletions, and renames must update the test and this inventory in the same change. Regenerate the summary, grouping, observations, and links with [`generate_builtin_loader_coverage.py`](../../tools/generate_builtin_loader_coverage.py); `--check` verifies that the committed document matches current test metadata, while `--add-test-plan-links --write` adds missing backlinks and writes the inventory.",
        "",
    ]
    return "\n".join(lines)


def add_test_plan_links(root: Path, tests: list[dict[str, object]]) -> int:
    """Insert the inventory backlink immediately after each shell shebang."""

    changed = 0
    for test in tests:
        path = root / test["path"]
        text = path.read_text(encoding="utf-8")
        lines = text.splitlines(keepends=True)
        matches = [
            index
            for index, line in enumerate(lines)
            if line.rstrip("\r\n") == TEST_PLAN_COMMENT
        ]
        if len(matches) > 1:
            raise RuntimeError(f"duplicate Test-plan backlink: {test['path']}")
        if matches:
            continue
        if not lines or lines[0].rstrip("\r\n") != "#!/bin/sh":
            raise RuntimeError(f"unexpected shell header: {test['path']}")
        newline = "\r\n" if lines[0].endswith("\r\n") else "\n"
        lines.insert(1, TEST_PLAN_COMMENT + newline)
        path.write_text("".join(lines), encoding="utf-8", newline="")
        changed += 1
    return changed


def validate_test_plan_links(tests: list[dict[str, object]], root: Path) -> None:
    """Reject missing, duplicate, or late reciprocal backlinks."""

    failures = []
    for test in tests:
        path = root / test["path"]
        lines = path.read_text(encoding="utf-8").splitlines()
        all_matches = [
            index + 1
            for index, line in enumerate(lines)
            if line == TEST_PLAN_COMMENT
        ]
        if len(all_matches) != 1 or all_matches[0] > 20:
            failures.append(
                f"{test['path']}: backlink lines {all_matches}, expected one in 1-20"
            )
    if failures:
        detail = failures[:50]
        if len(failures) > len(detail):
            detail.append(f"... {len(failures) - len(detail)} more failures")
        raise RuntimeError("\n".join(detail))


def check_document(path: Path, expected: str) -> bool:
    """Print a compact unified diff when the inventory is stale."""

    if not path.exists():
        print(f"missing generated inventory: {path}", file=sys.stderr)
        return False
    actual = path.read_text(encoding="utf-8")
    if actual == expected:
        return True
    diff = difflib.unified_diff(
        actual.splitlines(),
        expected.splitlines(),
        fromfile=str(path),
        tofile=f"{path} (generated)",
        n=2,
    )
    for line in list(diff)[:200]:
        print(line, file=sys.stderr)
    print("inventory is stale; run the generator with --write", file=sys.stderr)
    return False


def parse_args() -> argparse.Namespace:
    """Parse explicit read-only and updating modes."""

    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    parser.add_argument(
        "--add-test-plan-links",
        action="store_true",
        help="insert missing reciprocal comments before rendering",
    )
    args = parser.parse_args()
    if args.add_test_plan_links and not args.write:
        parser.error("--add-test-plan-links requires --write")
    return args


def main() -> int:
    """Update or verify the complete builtin-loader inventory."""

    args = parse_args()
    root = source_root()
    tests = read_tests(root)
    if args.add_test_plan_links:
        changed = add_test_plan_links(root, tests)
        if changed:
            tests = read_tests(root)
        print(f"added Test-plan backlinks: {changed}")
    validate_test_plan_links(tests, root)
    document = render_document(tests)
    output = root / PLAN_DOCUMENT
    if args.write:
        output.write_text(document, encoding="utf-8")
        print(f"wrote {output.relative_to(root)} with {len(tests)} tests")
        return 0
    return 0 if check_document(output, document) else 1


if __name__ == "__main__":
    raise SystemExit(main())
