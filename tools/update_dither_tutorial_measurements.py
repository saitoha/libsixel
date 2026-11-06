#!/usr/bin/env python3
"""Measure the dithering tutorial and render its result tables.

Without options, rewrite every measurement block of the tutorial pages from
docs/tutorial/dithering/measurements/results.json. With --check, verify that
the pages match the file. With --measure --lsqa PATH, first run the programs,
score their output with lsqa and write that file; --img2sixel and --sixel2png
add libsixel's result in both working color spaces. With --palettes
--img2sixel PATH, make the palettes of code/palettes again.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import platform
import re
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TUTORIAL = ROOT / "docs/tutorial/dithering"
CODE = TUTORIAL / "code"
CODEC = ROOT / "docs/tutorial/sixel-codec/code"
RESULTS = TUTORIAL / "measurements/results.json"
SNAKE = ROOT / "images/snake.ppm"

METHODS = (
    ("nearest", "Nearest color (step 1)", "step1", None),
    ("random", "Random nudge (step 2)", "step2", "random"),
    ("bayer", "Bayer pattern (step 2)", "step2", None),
    ("fs", "Floyd–Steinberg (step 3)", "step3", None),
    ("serpentine", "Floyd–Steinberg, serpentine (step 4)", "step4",
     "serpentine"),
    ("atkinson", "Atkinson (step 4)", "step4", "atkinson"),
    ("jajuni", "Jarvis, Judice, Ninke (step 4)", "step4", "jajuni"),
)
SETS = {
    "gradient-cube27": ("Gradient, 640×240, 27-color cube", "gradient",
                        "cube27"),
    "snake-cube27": ("Photograph, 600×450, 27-color cube", "snake", "cube27"),
    "snake-16": ("Photograph, 600×450, its 16-color palette", "snake",
                 "snake16"),
}
METRICS = ("MS-SSIM", "GMSD", "DE00_region_mean", "HighFreqRatio_delta",
           "StripeScore_rel", "BandingIndex_grad_rel")
PPDS = (16, 32, 60, 120)
LIGHT_SETS = ("gradient-cube27", "snake-cube27", "snake-16")
CLAMPS = ("soft", "hard", "none")
DEADZONES = (0, 5, 10, 20)
DEADZONE_SETS = {
    "flat": "Flat gray 100, 320×240, 16 grays",
    "snake16": "Photograph, 600×450, its 16-color palette",
    "snake256": "Photograph, 600×450, its 255-color palette",
    "cube27": "Photograph, 600×450, 27-color cube",
}
LOOKUPS = (("scan", "Compare with every color"),
           ("cache", "Remember recent colors"),
           ("memo", "One answer per cell"),
           ("grid", "A list of candidates per cell"))
THREADS = (1, 2, 4, 8, 14)
THREAD_METHODS = (("Floyd–Steinberg, 16 colors", "fs", "snake16"),
                  ("Floyd–Steinberg, 255 colors", "fs", "snake256"),
                  ("Bayer pattern, 16 colors", "bayer", "snake16"),
                  ("Bayer pattern, 255 colors", "bayer", "snake256"))
SEAM_BANDS = (("One band", 1, 0), ("Four bands", 4, 0),
              ("Four bands, 6 warm-up rows", 4, 6))
SEAM_SETS = {"gradient": "Gradient, 640×240", "flat": "Flat gray 64, 640×240",
             "snake": "Photograph, 600×450"}
PAGES = {
    "step4-kernels.md": ("speed",),
    "step5-good-dither.md": ("simple",),
    "step6-measuring.md": ("score-gradient-cube27", "score-snake-cube27",
                           "score-snake-16", "light", "ppd"),
    "measurements.md": ("environment", "speed", "simple",
                        "score-gradient-cube27", "score-snake-cube27",
                        "score-snake-16", "light", "ppd", "linear",
                        "carry-speed", "clamp", "deadzone", "lookup",
                        "threads", "seams"),
    "step7-linear-light.md": ("linear",),
    "step8-integer-error.md": ("carry-speed", "clamp", "deadzone"),
    "step9-lookup.md": ("lookup",),
    "step10-threads.md": ("threads", "seams"),
}
SIMPLE_RE = re.compile(r"pixel error ([\d.]+), area error ([\d.]+), "
                       r"flat runs ([\d.]+) \(original ([\d.]+)\)")
BENCH_RE = re.compile(r"\(([\d.]+) ns per pixel\)")
COMPARED_RE = re.compile(r"([\d.]+) colors compared per pixel")
SEAMS_RE = re.compile(r"([\d.]+) at band starts, ([\d.]+) elsewhere")


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    """Run a command, failing on a nonzero status."""
    return subprocess.run(command, check=True, capture_output=True, **kwargs)


def measure_all(arguments: argparse.Namespace) -> dict:
    """Run every program and score its output."""
    lsqa = str(arguments.lsqa)
    before = round(os.getloadavg()[0], 2)
    with tempfile.TemporaryDirectory(prefix="dither-measure-") as temp:
        out = Path(temp)
        run(["make", "-s", f"O={out}", "all"], cwd=CODE)
        codec = out / "codec"
        codec.mkdir()
        run(["make", "-s", f"O={codec}", "all"], cwd=CODEC)
        inputs = {"snake": SNAKE, "snake16": CODE / "palettes/snake16.ppm",
                  "snake256": CODE / "palettes/snake256.ppm"}
        for name, command in (("gradient", ("gradient", "640", "240")),
                              ("cube27", ("cube", "3"))):
            inputs[name] = out / f"input-{name}.ppm"
            inputs[name].write_bytes(run([str(out / "dtool"),
                                          *command]).stdout)
        results: dict[str, dict] = {"sets": {}, "ppd": {}, "speed": {}}
        for key, (_, picture, palette) in SETS.items():
            results["sets"][key] = {}
            for method, _, step, option in METHODS:
                command = [str(out / step), str(inputs[picture]),
                           str(inputs[palette])]
                if option:
                    command.append(option)
                path = out / "output.ppm"
                path.write_bytes(run(command).stdout)
                report = json.loads(run([lsqa, str(inputs[picture]),
                                         str(path)]).stdout)["quality"]
                simple = SIMPLE_RE.search(run(
                    [str(out / "dtool"), "measure", str(inputs[picture]),
                     str(path)]).stdout.decode())
                # The codec chapter's encoder: colors become registers.
                stream = run([str(codec / "step2"), "encode",
                              str(path)]).stdout
                entry = {name: report[name] for name in METRICS}
                entry.update(pixel_error=float(simple[1]),
                             area_error=float(simple[2]),
                             flat_runs=float(simple[3]),
                             flat_runs_original=float(simple[4]),
                             bytes=len(stream))
                results["sets"][key][method] = entry
        for method, _, step, option in METHODS:
            if method not in ("nearest", "bayer", "fs"):
                continue
            command = [str(out / step), str(inputs["gradient"]),
                       str(inputs["cube27"])]
            if option:
                command.append(option)
            path = out / "output.ppm"
            path.write_bytes(run(command).stdout)
            results["ppd"][method] = {}
            for ppd in PPDS:
                report = json.loads(run([lsqa, f"--ppd={ppd}",
                                         str(inputs["gradient"]),
                                         str(path)]).stdout)["quality"]
                results["ppd"][method][str(ppd)] = report["S-CIEDE2000"]
        results["light"] = measure_light(arguments, out, inputs)
        for method, _, step, option in METHODS:
            results["speed"][method] = {}
            for palette in ("snake16", "snake256"):
                best = 1e30
                for _ in range(arguments.rounds):
                    command = [str(out / step), "bench", str(SNAKE),
                               str(inputs[palette])]
                    if option:
                        command.append(option)
                    line = run(command).stdout.decode()
                    best = min(best, float(BENCH_RE.search(line)[1]))
                results["speed"][method][palette] = best
        results.update(measure_lessons(arguments, out, codec, inputs))

    def output(*command: str) -> str:
        return run(list(command), cwd=ROOT).stdout.decode().strip()
    machine = platform.machine()
    try:
        machine = output("sysctl", "-n", "machdep.cpu.brand_string")
    except (OSError, subprocess.CalledProcessError):
        pass
    results["environment"] = {
        "date": datetime.date.today().isoformat(),
        "revision": output("git", "rev-parse", "--short", "HEAD"),
        "machine": machine,
        "system": f"{platform.system()} {platform.release()}",
        "compiler": output("cc", "--version").splitlines()[0],
        "load_before": before,
        "load_after": round(os.getloadavg()[0], 2),
    }
    return results


def measure_light(arguments: argparse.Namespace, out: Path,
                  inputs: dict[str, Path]) -> dict:
    """Score Floyd-Steinberg in stored numbers and in linear light."""
    report = {}
    command = [str(out / "step3"), str(SNAKE), str(inputs["cube27"])]
    path = out / "light.ppm"
    path.write_bytes(run(command).stdout)
    score = [str(arguments.lsqa), "-m", "DE00_region_mean", str(SNAKE)]
    report["tutorial"] = float(run(score + [str(path)]).stdout)
    if arguments.img2sixel is None or arguments.sixel2png is None:
        raise SystemExit("--measure needs --img2sixel and --sixel2png")
    palette = out / "cube27.png"
    palette.write_bytes(png_palette(inputs["cube27"]))
    for space in ("gamma", "linear"):
        stream = out / f"light-{space}.six"
        stream.write_bytes(run([str(arguments.img2sixel), "-W", space, "-m",
                                str(palette), "-d", "fs",
                                str(ROOT / "images/snake.png")]).stdout)
        decoded = out / f"light-{space}.png"
        run([str(arguments.sixel2png), "-i", str(stream), "-o", str(decoded)])
        report[space] = float(run(score + [str(decoded)]).stdout)
    return report


def bench(command: list[str], rounds: int, **kwargs) -> float:
    """The fastest time per pixel of several runs of a bench command."""
    return min(float(BENCH_RE.search(run(command, **kwargs).stdout.decode())[1])
               for _ in range(rounds))


def measure_lessons(arguments: argparse.Namespace, out: Path, codec: Path,
                    inputs: dict[str, Path]) -> dict:
    """Measure lessons 7 to 10."""
    rounds = arguments.rounds
    path = out / "lesson.ppm"
    extra = dict(inputs)
    for name, command in (("flat100", ("flat", "320", "240", "100", "100",
                                       "100")),
                          ("flat64", ("flat", "640", "240", "64", "64", "64")),
                          ("gray16", ("gray", "16"))):
        extra[name] = out / f"input-{name}.ppm"
        extra[name].write_bytes(run([str(out / "dtool"), *command]).stdout)

    def produce(step: str, picture: str, palette: str,
                option: str | None) -> subprocess.CompletedProcess:
        command = [str(out / step), str(extra[picture]), str(extra[palette])]
        if option is not None:
            command.append(option)
        done = run(command)
        path.write_bytes(done.stdout)
        return done

    def score(picture: str) -> dict:
        report = json.loads(run([str(arguments.lsqa), str(extra[picture]),
                                 str(path)]).stdout)["quality"]
        stream = run([str(codec / "step2"), "encode", str(path)]).stdout
        return {"area": report["DE00_region_mean"],
                "ms_ssim": report["MS-SSIM"], "bytes": len(stream)}

    result: dict[str, dict] = {"linear": {}, "carry_speed": {}, "clamp": {},
                               "deadzone": {}, "lookup": {}, "threads": {},
                               "seams": {}}
    for key in LIGHT_SETS:
        _, picture, palette = SETS[key]
        result["linear"][key] = {}
        for step in ("step3", "step7"):
            produce(step, picture, palette, None)
            result["linear"][key][step] = score(picture)
    for palette in ("snake16", "snake256"):
        result["carry_speed"][palette] = {
            step: bench([str(out / step), "bench", str(SNAKE),
                         str(extra[palette])], rounds)
            for step in ("step3", "step8")}
    for clamp in CLAMPS:
        produce("step8", "snake", "snake16", f"clamp={clamp}")
        result["clamp"][clamp] = score("snake")
    for key, (picture, palette) in {"flat": ("flat100", "gray16"),
                                    "snake16": ("snake", "snake16"),
                                    "snake256": ("snake", "snake256"),
                                    "cube27": ("snake", "cube27")}.items():
        result["deadzone"][key] = {}
        for zone in DEADZONES:
            produce("step8", picture, palette, f"deadzone={zone}")
            result["deadzone"][key][str(zone)] = score(picture)
    for palette in ("snake16", "snake256"):
        result["lookup"][palette] = {}
        reference = run([str(out / "step3"), str(SNAKE),
                         str(extra[palette])]).stdout
        for method, _ in LOOKUPS:
            done = produce("step9", "snake", palette, f"{method},stats")
            pixels = len(reference) // 3
            result["lookup"][palette][method] = {
                "ns": bench([str(out / "step9"), "bench", str(SNAKE),
                             str(extra[palette]), method], rounds),
                "compared": float(COMPARED_RE.search(done.stderr.decode())[1]),
                "changed": sum(reference[i:i + 3] != done.stdout[i:i + 3]
                               for i in range(0, len(reference), 3)) / pixels,
                "area": score("snake")["area"],
            }
    for _, method, palette in THREAD_METHODS:
        entry = result["threads"].setdefault(f"{method}-{palette}", {})
        for count in THREADS:
            environment = dict(os.environ, TUTORIAL_THREADS=str(count))
            entry[str(count)] = bench(
                [str(out / "step10"), "bench", str(SNAKE),
                 str(extra[palette]), f"{method},bands=14"], rounds,
                env=environment)
    for picture, source in (("gradient", "gradient"), ("flat", "flat64"),
                            ("snake", "snake")):
        result["seams"][picture] = {}
        height = int(re.match(rb"P[36]\s+\d+\s+(\d+)",
                              extra[source].read_bytes())[1])
        band = ((height + 3) // 4 + 5) // 6 * 6
        for _, bands, overlap in SEAM_BANDS:
            produce("step10", source, "snake16",
                    f"bands={bands},overlap={overlap}")
            match = SEAMS_RE.search(run([str(out / "dtool"), "seams",
                                         str(path), str(band)]).stdout.decode())
            scores = score(source)
            result["seams"][picture][f"{bands}-{overlap}"] = {
                "start": float(match[1]), "elsewhere": float(match[2]),
                "bytes": scores["bytes"], "area": scores["area"]}
    return result


def png_palette(path: Path) -> bytes:
    """Convert a binary PPM palette into a PNG that img2sixel can read."""
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", path.read_bytes())
    data = path.read_bytes()[match.end():]
    width, height = int(match[1]), int(match[2])

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", zlib.crc32(kind + payload)))
    raw = b"".join(b"\0" + data[y * width * 3:(y + 1) * width * 3]
                   for y in range(height))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0,
                                         0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def make_palettes(img2sixel: Path) -> None:
    """Make the palettes of code/palettes with libsixel."""
    with tempfile.TemporaryDirectory(prefix="dither-palettes-") as temp:
        run(["make", "-s", f"O={temp}", "all"], cwd=CODEC)
        for count in (16, 256):
            stream = run([str(img2sixel), "-p", str(count), "-d", "none",
                          str(ROOT / "images/snake.png")]).stdout
            data = run([f"{temp}/step2", "decode"], input=stream).stdout
            match = re.match(rb"P6\s+\d+\s+\d+\s+255\s", data)
            pixels = data[match.end():]
            colors = sorted({tuple(pixels[i:i + 3])
                             for i in range(0, len(pixels), 3)})
            lines = ["P3",
                     f"# {len(colors)} colors chosen by libsixel for "
                     "images/snake.png",
                     f"{len(colors)} 1", "255"]
            lines += ["%d %d %d" % color for color in colors]
            (CODE / f"palettes/snake{count}.ppm").write_text(
                "\n".join(lines) + "\n", encoding="utf-8")


def table(header: list[str], rows: list[list[str]]) -> str:
    """Format a Markdown table."""
    lines = ["| " + " | ".join(header) + " |",
             "| " + " | ".join("---" for _ in header) + " |"]
    lines += ["| " + " | ".join(row) + " |" for row in rows]
    return "\n".join(lines)


def label(method: str) -> str:
    """The name of a method in a table."""
    return next(entry[1] for entry in METHODS if entry[0] == method)


def render_environment(results: dict) -> str:
    """The conditions of the measurement."""
    env = results["environment"]
    return table(["Item", "Value"], [
        ["Date", env["date"]],
        ["Based on revision", f"`{env['revision']}`"],
        ["Machine", env["machine"]],
        ["System", env["system"]],
        ["Compiler", env["compiler"]],
        ["Load average before, after",
         f"{env['load_before']}, {env['load_after']}"],
    ])


def render_speed(results: dict) -> str:
    """Time per pixel of every method."""
    return table(["Method", "16 colors, ns per pixel",
                  "256 colors, ns per pixel"], [
        [label(method), f"{results['speed'][method]['snake16']:.1f}",
         f"{results['speed'][method]['snake256']:.1f}"]
        for method, *_ in METHODS])


def render_simple(results: dict) -> str:
    """Lesson 5's three numbers."""
    rows = []
    for key in ("gradient-cube27", "snake-16"):
        for method in ("nearest", "bayer", "fs", "atkinson"):
            cell = results["sets"][key][method]
            rows.append([SETS[key][0], label(method),
                         f"{cell['pixel_error']:.1f}",
                         f"{cell['area_error']:.1f}",
                         f"{cell['flat_runs']:.1f}"])
        original = results["sets"][key]["nearest"]["flat_runs_original"]
        rows.append([SETS[key][0], "The original itself", "0.0", "0.0",
                     f"{original:.1f}"])
    return table(["Picture", "Method", "Pixel error", "Area error",
                  "Flat runs"], rows)


def render_score(results: dict, key: str) -> str:
    """lsqa's scores for one picture and palette."""
    rows = []
    for method, *_ in METHODS:
        cell = results["sets"][key][method]
        rows.append([label(method), f"{cell['MS-SSIM']:.3f}",
                     f"{cell['GMSD']:.3f}",
                     f"{cell['DE00_region_mean']:.2f}",
                     f"{cell['HighFreqRatio_delta']:+.3f}",
                     f"{cell['StripeScore_rel']:+.2f}",
                     f"{cell['BandingIndex_grad_rel']:+.3f}",
                     f"{cell['bytes']:,}"])
    return table(["Method", "MS-SSIM", "GMSD", "Area ΔE00", "High freq.",
                  "Stripes", "Banding", "Bytes"], rows)


def render_ppd(results: dict) -> str:
    """S-CIEDE2000 against the assumed viewing density."""
    return table(["Method"] + [f"{ppd} pixels per degree" for ppd in PPDS], [
        [label(method)] + [f"{results['ppd'][method][str(ppd)]:.2f}"
                           for ppd in PPDS]
        for method, *_ in METHODS if method in results["ppd"]])


def render_light(results: dict) -> str:
    """Area error of Floyd-Steinberg in each working space."""
    light = results["light"]
    return table(["Floyd–Steinberg on the photograph, 27-color cube",
                  "Area ΔE00"], [
        ["Tutorial step 3, on the stored numbers", f"{light['tutorial']:.2f}"],
        ["libsixel, `-W gamma`", f"{light['gamma']:.2f}"],
        ["libsixel, `-W linear`", f"{light['linear']:.2f}"],
    ])


def render_linear(results: dict) -> str:
    """Stored numbers against linear light, in the tutorial's own programs."""
    rows = []
    for key in LIGHT_SETS:
        stored = results["linear"][key]["step3"]
        light = results["linear"][key]["step7"]
        rows.append([SETS[key][0], f"{stored['area']:.2f} → {light['area']:.2f}",
                     f"{stored['ms_ssim']:.3f} → {light['ms_ssim']:.3f}",
                     f"{stored['bytes']:,} → {light['bytes']:,}"])
    return table(["Picture and palette", "Area ΔE00", "MS-SSIM", "Bytes"], rows)


def render_carry_speed(results: dict) -> str:
    """Floats against whole numbers."""
    return table(["Palette", "Floats (step 3), ns per pixel",
                  "Whole numbers (step 8), ns per pixel"], [
        [name, f"{results['carry_speed'][key]['step3']:.1f}",
         f"{results['carry_speed'][key]['step8']:.1f}"]
        for name, key in (("16 colors", "snake16"), ("255 colors", "snake256"))
    ])


def render_clamp(results: dict) -> str:
    """The three limits on one picture."""
    names = {"soft": "`soft`", "hard": "`hard`", "none": "`none`"}
    return table(["Limit", "Area ΔE00", "MS-SSIM", "Bytes"], [
        [names[clamp], f"{results['clamp'][clamp]['area']:.2f}",
         f"{results['clamp'][clamp]['ms_ssim']:.3f}",
         f"{results['clamp'][clamp]['bytes']:,}"] for clamp in CLAMPS])


def render_deadzone(results: dict) -> str:
    """The deadzone against the picture and the palette."""
    rows = []
    for key, name in DEADZONE_SETS.items():
        for zone in DEADZONES:
            cell = results["deadzone"][key][str(zone)]
            rows.append([name, str(zone), f"{cell['area']:.2f}",
                         f"{cell['ms_ssim']:.3f}", f"{cell['bytes']:,}"])
    return table(["Picture and palette", "Deadzone", "Area ΔE00", "MS-SSIM",
                  "Bytes"], rows)


def render_lookup(results: dict) -> str:
    """Four ways to find the nearest color."""
    rows = []
    for key, palette in (("snake16", "16 colors"), ("snake256", "255 colors")):
        for method, name in LOOKUPS:
            cell = results["lookup"][key][method]
            rows.append([palette, name, f"{cell['ns']:.1f}",
                         f"{cell['compared']:.1f}",
                         f"{cell['changed'] * 100:.1f}%",
                         f"{cell['area']:.3f}"])
    return table(["Palette", "Method", "ns per pixel",
                  "Colors compared per pixel", "Pixels changed", "Area ΔE00"],
                 rows)


def render_threads(results: dict) -> str:
    """Time against the number of threads."""
    rows = []
    for name, method, palette in THREAD_METHODS:
        cell = results["threads"][f"{method}-{palette}"]
        one = cell["1"]
        rows.append([name] + [f"{cell[str(count)]:.1f} ({one / cell[str(count)]:.1f}×)"
                              for count in THREADS])
    return table(["14 bands, ns per pixel"]
                 + [f"{count} threads" if count > 1 else "1 thread"
                    for count in THREADS], rows)


def render_seams(results: dict) -> str:
    """What a band start does to the picture."""
    rows = []
    for key, picture in SEAM_SETS.items():
        for name, bands, overlap in SEAM_BANDS:
            cell = results["seams"][key][f"{bands}-{overlap}"]
            rows.append([picture, name, f"{cell['start']:.0f}",
                         f"{cell['elsewhere']:.0f}", f"{cell['bytes']:,}",
                         f"{cell['area']:.2f}"])
    return table(["Picture, 16-color palette", "Bands",
                  "Color changes per row, band starts",
                  "Color changes per row, elsewhere", "Bytes", "Area ΔE00"],
                 rows)


def render(results: dict, block: str) -> str:
    """Return the Markdown of one measurement block."""
    if block.startswith("score-"):
        return render_score(results, block[len("score-"):])
    return {"environment": render_environment, "speed": render_speed,
            "simple": render_simple, "ppd": render_ppd,
            "light": render_light, "linear": render_linear,
            "carry-speed": render_carry_speed, "clamp": render_clamp,
            "deadzone": render_deadzone, "lookup": render_lookup,
            "threads": render_threads, "seams": render_seams}[block](results)


def update_pages(check: bool) -> None:
    """Rewrite or verify every measurement block."""
    results = json.loads(RESULTS.read_text(encoding="utf-8"))
    stale = []
    for page, blocks in PAGES.items():
        path = TUTORIAL / page
        source = path.read_text(encoding="utf-8")
        updated = source
        for block in blocks:
            begin = f"<!-- measurements:begin:{block} -->"
            end = "<!-- measurements:end -->"
            if begin not in updated:
                raise SystemExit(f"{page}: missing block {block}")
            content = f"{begin}\n{render(results, block)}\n{end}"
            updated = re.sub(re.escape(begin) + r".*?" + re.escape(end),
                             lambda _: content, updated, count=1, flags=re.S)
        if updated != source:
            stale.append(page)
            if not check:
                path.write_text(updated, encoding="utf-8")
    if check and stale:
        raise SystemExit("stale measurement tables: " + ", ".join(stale))


def main() -> None:
    """Measure and render as requested."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--measure", action="store_true")
    parser.add_argument("--lsqa", type=Path, help="the lsqa program")
    parser.add_argument("--img2sixel", type=Path)
    parser.add_argument("--sixel2png", type=Path)
    parser.add_argument("--palettes", action="store_true")
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--no-pages", action="store_true")
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    if arguments.palettes:
        if arguments.img2sixel is None:
            parser.error("--palettes needs --img2sixel")
        make_palettes(arguments.img2sixel)
        return
    if arguments.measure:
        if arguments.lsqa is None:
            parser.error("--measure needs --lsqa")
        results = measure_all(arguments)
        RESULTS.parent.mkdir(exist_ok=True)
        RESULTS.write_text(json.dumps(results, indent=2, sort_keys=True)
                           + "\n", encoding="utf-8")
    if not arguments.no_pages:
        update_pages(arguments.check)


if __name__ == "__main__":
    main()
