#!/usr/bin/env python3
"""Record private progressive observations with provenance and optional QA.

Repair-only branches compare legacy upper/full selection with an experimental
finite threshold ladder. Palette cap and noise are controlled separately; the
ordinary baseline uses no dither. Timings are instrumented, exclude independent
decode/prediction/QA, and are not production latency benchmarks.
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import subprocess


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(root: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def program(path: Path) -> dict:
    payload = path.parent / ".libs" / path.name
    return {
        "path": str(path), "sha256": digest(path),
        "payload_sha256": digest(payload) if payload.is_file() else digest(path),
    }


def prediction_report(records: list[dict]) -> dict:
    """Validate state pairing and compare raw-sample prediction errors."""
    steps = {r["sequence"]: r for r in records if r["event"] == "step"}
    errors = {key: [] for key in ("systematic_256", "uniform_hash_256",
              "stratified_total256", "stratified_16", "stratified_256")}
    counts = {key: [] for key in errors}
    priorities = []
    distributions = {}
    seen_predictions = set()
    seen_priorities = set()
    for record in records:
        if record["event"] not in ("distribution", "prediction", "priority"):
            continue
        step = steps[record["sequence"]]
        require(step["committed"] and step["audit_complete"] and
                record["generation"] == step["generation"] and
                record["before_revision"] == step["before_revision"],
                "counterfactual snapshot mismatch")
        if record["event"] == "distribution":
            require(record["sequence"] not in distributions, "duplicate distribution")
            distributions[record["sequence"]] = record
            require(len(record["bands"]) == len(step["bands"]), "band count mismatch")
            positions = set()
            for index, band in enumerate(record["bands"]):
                require(band["population"] == step["bands"][index][0] and
                        len(band["samples"]) == min(256, band["population"]),
                        "reservoir population mismatch")
                for sample in band["samples"]:
                    require(sample[0] not in positions and
                            all(math.isfinite(v) for v in sample),
                            "duplicate or nonfinite stratified sample")
                    positions.add(sample[0])
                    require((index == 0 or sample[4] > step["band_upper"][index - 1]) and
                            (index == 15 or sample[4] <= step["band_upper"][index]),
                            "sample in wrong residual band")
            continue
        if record["event"] == "priority":
            key = (record["sequence"], record["policy"])
            require(key not in seen_priorities, "duplicate priority comparison")
            seen_priorities.add(key)
            require(record["painted"] <= record["paint_cap"] == step["committed_pixels"] and
                    record["wire_bytes"] is None,
                    "priority comparison is not a measured byte budget")
            priorities.append(record)
            continue
        require(record["sequence"] in distributions, "missing raw distribution")
        key = (record["sequence"], record["mask_squared"], record["quota"])
        require(key not in seen_predictions, "duplicate prediction")
        seen_predictions.add(key)
        require(record["maximum_lower"] <= record["reference_maximum"] + 1e-12 and
                record["maximum_upper"] + 1e-12 >= record["reference_maximum"],
                "invalid maximum bounds")
        if record["exact"]:
            require(math.isclose(record["gain"], record["reference_gain"], abs_tol=1e-9),
                    "exhaustive predictor differs from pixel reference")
        # Report the encoder's actual mask separately from broader-mask
        # counterfactuals. Relative errors with a zero denominator stay absent.
        if record["mask_squared"] != step["mask_squared"] or not record["reference_gain"]:
            continue
        key = f"stratified_{record['quota']}" if record["quota"] else "stratified_total256"
        errors[key].append(abs(record["gain"] / record["reference_gain"] - 1))
        counts[key].append(record["samples"])
        if record["quota"] == 256 and step["samples"]:
            # Each band's bottom-256 reservoir contains every member of the
            # global bottom-256 set, allowing a matched hash-sampling control.
            pool = [s for b in distributions[record["sequence"]]["bands"] for s in b["samples"]]
            uniform = sorted(pool, key=lambda s: (position_priority(s[0]), s[0]))[:256]
            for key, samples in (("systematic_256", step["samples"]),
                                 ("uniform_hash_256", uniform)):
                gain = 0.0
                for sample in samples:
                    before = sample[4]
                    if before <= step["stop_squared"] or before < step["mask_squared"]:
                        continue
                    after = min(sum((sample[i + 1] - color[i]) ** 2 for i in range(3))
                                for color in step["palette"])
                    if before - after > 1e-12 + 1e-7 * max(before, after):
                        gain += before - after
                gain *= step["eligible"] / len(samples)
                errors[key].append(abs(gain / record["reference_gain"] - 1))
                counts[key].append(len(samples))
    committed = {sequence: step for sequence, step in steps.items() if step["committed"]}
    require(set(distributions) == set(committed), "incomplete distributions")
    require(seen_predictions == {(sequence, mask, quota) for sequence, step in committed.items()
                                 for mask in {step["mask_squared"], step["stop_squared"]}
                                 for quota in (0, 16, 256)}, "incomplete predictions")
    require(seen_priorities == {(sequence, policy) for sequence in committed
                               for policy in ("tail", "aggregate", "target_excess")},
            "incomplete priority comparisons")
    return {
        "fixed_actual_mask_gain_errors": {
            key: {"n": len(values), "mean_absolute_relative_error":
                  sum(values) / len(values) if values else None,
                  "worst_absolute_relative_error": max(values) if values else None,
                  "mean_samples": sum(counts[key]) / len(values) if values else None}
            for key, values in errors.items()
        },
        "same_state_equal_paint_comparisons": priorities,
    }


def position_priority(position: int) -> int:
    mask = (1 << 64) - 1
    value = (position + 0x9e3779b97f4a7c15) & mask
    value = ((value ^ (value >> 30)) * 0xbf58476d1ce4e5b9) & mask
    value = ((value ^ (value >> 27)) * 0x94d049bb133111eb) & mask
    return value ^ (value >> 31)


def validate_ladder(records: list[dict], policy: str, allowance: int) -> None:
    """Check frozen thresholds and packet accounting across resumable calls."""
    knots = None
    rung = 0
    emissions = [0] * 5
    noise_off = False
    for step in records:
        if step["event"] != "step":
            continue
        state = step["ladder"]
        if policy == "legacy":
            require(state["allowance"] == 0 and not state["thresholds"],
                    "legacy branch unexpectedly enabled ladder")
            continue
        require(state["allowance"] == allowance, "ladder allowance changed")
        values = state["thresholds"]
        if not values:
            require(knots is None and not step["committed"], "ladder reset")
            continue
        require(1 <= len(values) <= 5 and
                all(math.isfinite(v) for v in values) and
                all(a > b for a, b in zip(values, values[1:])) and
                values[-1] == step["stop_squared"], "invalid ladder thresholds")
        if knots is None:
            knots = values
        require(values == knots, "thresholds changed within a binding")
        current = state["rung"]
        require(state["reason"] in (0, 1, 2, 3), "unknown rung entry reason")
        require(rung <= current < len(knots), "ladder moved backward")
        require(not noise_off or state["noise_off"], "noise was reenabled")
        noise_off = state["noise_off"]
        rung = current
        final = current + 1 == len(knots)
        require(state["emissions_before"] == (0 if final else emissions[current]),
                "yield, refit or failure consumed a packet allowance")
        require(final or emissions[current] < allowance, "rung allowance exceeded")
        if step["selection_complete"]:
            require(step["mask_squared"] == knots[current], "mask/rung mismatch")
        if final:
            require(noise_off and step["full_selection"] and step["zero_noise"],
                    "final rung did not enter full no-noise recovery")
            require(step["noise_strength"] == 0, "final rung applied noise")
        if step["committed"]:
            emissions[current] += 1


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path, help="new directory outside the repository")
    parser.add_argument("--study", type=Path, default=root / "tools/progressive-study")
    parser.add_argument("--lsqa", type=Path, default=root / "assessment/lsqa")
    parser.add_argument("--passes", type=int, default=10, help="repair packets after baseline")
    parser.add_argument("--seconds", type=float, default=10)
    parser.add_argument("--packet-bytes", type=int, default=16384)
    parser.add_argument("--workspace", default="gamma",
                        choices=["gamma", "linear", "oklab", "cielab", "din99d"])
    parser.add_argument("--noise", type=float, action="append")
    parser.add_argument("--selection-fraction", type=float, action="append",
                        help="legacy upper-selection fraction (repeatable, legacy only)")
    parser.add_argument("--colors", type=int, action="append",
                        help="repair palette cap (repeatable)")
    parser.add_argument("--policy", action="append", choices=["legacy", "ladder"])
    parser.add_argument("--stage-passes", type=int, choices=[1, 2, 4], default=2,
                        help="accepted packets per nonfinal ladder rung")
    parser.add_argument("--quality", action="store_true")
    parser.add_argument("--grade", type=float, default=2, help="exploratory DE00 maximum target")
    parser.add_argument("--ms-ssim", type=float, default=0.98)
    args = parser.parse_args()
    args.input = args.input.resolve(strict=True)
    args.output = args.output.resolve()
    args.study = args.study.resolve(strict=True)
    args.lsqa = args.lsqa.resolve(strict=True)
    noises = args.noise if args.noise is not None else [0.055, 0.0]
    fractions = args.selection_fraction if args.selection_fraction is not None else [0.25]
    color_caps = args.colors if args.colors is not None else [32]
    policies = args.policy if args.policy is not None else ["legacy"]
    if (args.output.is_relative_to(root) or args.output.exists() or
            not 0 < args.passes <= 10000 or not math.isfinite(args.seconds) or
            args.seconds <= 0 or not 0 < args.packet_bytes <= 2147483647 or
            any(not math.isfinite(n) or not 0 <= n <= 1 for n in noises) or
            any(not math.isfinite(f) or not 0 < f <= 1 for f in fractions) or
            any(not 1 <= c <= 255 for c in color_caps) or
            len(set(fractions)) != len(fractions) or len(set(color_caps)) != len(color_caps) or
            len(set(policies)) != len(policies) or ("ladder" in policies and fractions != [0.25]) or
            len(set(noises)) != len(noises) or not math.isfinite(args.grade) or
            args.grade < 0 or not 0 <= args.ms_ssim < 1):
        parser.error("invalid limits, duplicate noise, or output directory already exists/in repository")
    args.output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("SIXEL_", "LIBSIXEL_"))}
    env.update(LC_ALL="C", TZ="UTC")
    patch = subprocess.check_output(["git", "-C", str(root), "diff", "--binary", "HEAD"])
    (args.output / "source.patch").write_bytes(patch)
    changed = git(root, "diff", "--name-only", "HEAD").splitlines()
    untracked = git(root, "ls-files", "--others", "--exclude-standard",
                    "src", "include", "tools", "tests/processing/progressive").splitlines()
    source_hashes = {p: digest(root / p) for p in sorted(set(changed + untracked))
                     if (root / p).is_file()}
    library_dir = args.study.parent.parent / "src"
    libraries = sorted(set(library_dir.glob(".libs/libsixel*.dylib")) |
                       set(library_dir.glob(".libs/libsixel*.so*")) |
                       set(library_dir.glob("libsixel*.so*")) |
                       set(library_dir.glob("libsixel*.dylib")))
    metadata = {
        "schema_version": 3, "revision": git(root, "rev-parse", "HEAD"),
        "source_patch_sha256": hashlib.sha256(patch).hexdigest(),
        "changed_and_untracked_source_sha256": source_hashes,
        "input": {"path": str(args.input), "sha256": digest(args.input)},
        "study": program(args.study), "lsqa": program(args.lsqa),
        "build_libraries": {str(p): digest(p) for p in libraries if p.is_file()},
        "protocol": {
            "workspace": args.workspace, "output_colorspace": "gamma/sRGB",
            "threads": 1, "gpu": "off", "lookup": "none",
            "baseline_dither": "none", "repair_noise": noises,
            "repair_selection_fraction": fractions, "repair_color_cap": color_caps,
            "repair_policy": policies, "ladder_stage_passes": args.stage_passes,
            "ladder_entry_reasons": ["initial", "packet allowance", "empty mask", "no paint"],
            "ladder": "frozen initial residual quantiles 7/8,3/4,1/2,1/4,exact stop; final rung disables noise",
            "branching": "identical baseline; later states follow each branch's own commits",
            "max_repairs": args.passes, "max_repair_seconds": args.seconds,
            "packet_bytes": args.packet_bytes, "sixel_environment_removed": True,
            "timing": "instrumented encoder plus sink; independent observation excluded",
            "seconds_limit": "repair work, checked between bounded step calls",
            "quality": args.quality, "quality_signal": "gamma/float32",
            "error_definition": "squared Euclidean distance in the binding workspace",
            "phase_order": ["audit", "select", "palette", "apply", "rescue", "packet", "write", "commit"],
            "phase_cost_units": ["algorithm_pixel_visits", "algorithm_distances", "instrumented_seconds"],
            "observer_distances": "additional evaluations for the current packet proposal, not budget counters",
            "sample_kind": "systematic positions with recorded stride; no full-population weights",
            "sample_fields": ["position", "target_w0", "target_w1", "target_w2", "before_squared_error"],
            "validity": "audit_complete, selection_complete, proposal_complete, committed; zero bytes before serialization is unknown",
            "palette": "actual wire-projected palette in workspace, including unused constructed entries",
            "prediction": "no-dither uncapped fixed-palette counterfactual; exact pixel scan as reference",
            "distribution": "16 residual bands, exact populations, bottom-256 SplitMix64 position hashes per band",
            "prediction_quotas": [0, 16, 256],
            "quota_zero": "total 256 samples shared equally among occupied bands with redistribution",
            "sampling_controls": "systematic 256, uniform bottom-256 hashes, stratified total 256",
            "prediction_weights": "band population divided by retained sample count; estimates not clipped",
            "prediction_maximum": "sample lower bound and unsampled-band upper bound; not an estimated certificate",
            "priority_comparison": "same-state/palette, equal painted-pixel caps; bytes unknown, no policy winner implied",
            "quality_region_window": 16, "ms_ssim": "reference five-scale product; minimum 176x176",
            "target": {"DE00_max": args.grade, "MS-SSIM_min": args.ms_ssim},
            "target_status": "exploratory numeric predicate, not a universal viewing grade",
        },
    }
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    summary = []
    baseline_bytes = None
    branches = itertools.product(noises, fractions, color_caps, policies)
    for index, (noise, fraction, colors, policy) in enumerate(branches):
        capture = args.output / f"run-{index}.six"
        trace = args.output / f"run-{index}.jsonl"
        command = [str(args.study), str(args.input), str(capture), str(args.passes),
                   str(args.seconds), str(noise), str(args.packet_bytes), args.workspace,
                   str(fraction), str(colors), policy, str(args.stage_passes)]
        with trace.open("w") as output:
            subprocess.run(command, env=env, stdout=output, check=True,
                           timeout=max(120, args.seconds * 10 + 60))
        records = [json.loads(line) for line in trace.read_text().splitlines()]
        require(records[0]["event"] == "baseline" and records[-1]["event"] == "end",
                "incomplete study trace")
        stream = capture.read_bytes()
        baseline = records[0]
        prefix = stream[:baseline["wire_bytes"]]
        if baseline_bytes is None:
            baseline_bytes = prefix
        require(prefix == baseline_bytes, "baseline changed across repair-only branches")
        sequence = 0
        for record in records:
            if record["event"] != "step":
                continue
            sequence += 1
            require(record["sequence"] == sequence and record["version"] == 3,
                    "observation sequence gap or schema mismatch")
            require(record["timing_valid"], "invalid timing sample")
            if record["audit_complete"]:
                require(sum(b[0] for b in record["bands"]) == record["eligible"],
                        "audit histogram population mismatch")
            if record["committed"]:
                require(sum(map(sum, record["transitions"])) == record["committed_pixels"],
                        "committed transition population mismatch")
                require(record["committed_bytes"] <= args.packet_bytes, "packet budget exceeded")
                require(record["committed_gain"] > 0, "non-improving committed repair")
        require(records[-1]["wire_bytes"] == len(stream), "capture size mismatch")
        diagnostics = prediction_report(records)
        validate_ladder(records, policy, args.stage_passes)
        (args.output / f"run-{index}-prediction.json").write_text(
            json.dumps(diagnostics, indent=2) + "\n")
        evaluations = []
        reached = None
        encoder_seconds = baseline["seconds"]
        for record in records:
            if record["event"] == "step":
                encoder_seconds += record["step_seconds"]
            if record["event"] not in ("baseline", "decoded"):
                continue
            item = dict(record, encoder_seconds=encoder_seconds)
            if args.quality:
                qa_command = [str(args.lsqa), "-L", "builtin!", "-#", "builtin",
                              "-M", "6delta-progressive", "-T", "srgb",
                              "-W", "gamma", "-P", "float32", "-w", "16",
                              str(args.input), "-"]
                qa = subprocess.run(qa_command, env=env,
                                    input=stream[:record["wire_bytes"]],
                                    capture_output=True, check=True, timeout=120)
                item["assessment"] = json.loads(qa.stdout)
                quality = item["assessment"]["quality"]
                de00 = quality["DE00_max"]
                ssim = quality["MS-SSIM"]
                item["target_reached"] = (de00 is not None and ssim is not None and
                                          de00 <= args.grade and ssim >= args.ms_ssim)
                if item["target_reached"] and reached is None:
                    reached = {"bytes": record["wire_bytes"],
                               "passes": record.get("repair", 0) + 1,
                               "encoder_seconds": encoder_seconds}
            evaluations.append(item)
        report = {"command": command, "noise": noise, "end": records[-1],
                  "selection_fraction": fraction, "color_cap": colors,
                  "policy": policy, "stage_passes": args.stage_passes,
                  "prediction_errors": diagnostics["fixed_actual_mask_gain_errors"],
                  "quality_evaluated": args.quality,
                  "first_observed_target": reached, "evaluations": evaluations}
        (args.output / f"run-{index}-quality.json").write_text(json.dumps(report, indent=2) + "\n")
        summary.append({k: v for k, v in report.items() if k != "evaluations"})
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
