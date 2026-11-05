#!/usr/bin/env python3
"""Measure the SIXEL codec tutorial and render its result tables.

Without options, rewrite every measurement block of the tutorial pages from
docs/tutorial/sixel-codec/measurements/results.json. With --check, verify
that the pages match the file. With --measure, first run the benchmarks and
write that file: `make bench` for every step, then copies of the sources
patched with phase timers and single-technique variants, a thread sweep, and
optionally the same pictures through a libsixel build.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TUTORIAL = ROOT / "docs/tutorial/sixel-codec"
CODE = TUTORIAL / "code"
RESULTS = TUTORIAL / "measurements/results.json"
INPUTS = (
    ("photo-small", "Photograph, 600×450"),
    ("photo-fhd", "Photograph, 1920×1080"),
    ("two", "Two colors, 1920×1080"),
    ("noise", "Random noise, 1920×1080"),
)
PAGES = {
    "README.md": ("summary",),
    "step1-run-length.md": ("step1",),
    "step2-nodes.md": ("step2",),
    "step3-fewer-decisions.md": ("step3", "attribution"),
    "step4-column-words.md": ("step4",),
    "step5-simd.md": ("step5",),
    "step6-threads.md": ("step6",),
    "measurements.md": ("environment", "attribution", "phases", "materialize",
                        "prescan", "threads", "libsixel"),
}
BENCH_RE = re.compile(
    r"(?P<colors>\d+) colors, (?P<bytes>\d+) bytes: encode (?P<enc>[\d.]+) ms"
    r".*decode (?P<dec>[\d.]+) ms")
TIMER_HEADER = """
#include <stdio.h>
#include <time.h>
static double pt_sum[5];
static double pt_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
"""
LIBSIXEL_BENCH = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sixel.h>
static size_t bytes;
static int sink(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    bytes += (size_t)size;
    return size;
}
static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    int threads = atoi(argv[2]), runs = atoi(argv[3]);
    unsigned char *stream, *pixels = NULL, *palette = NULL, *copy;
    int width = 0, height = 0, ncolors = 0, i;
    double enc = 1e30, dec = 1e30, t;
    long size;

    (void)argc;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);
    stream = malloc((size_t)size);
    if (fread(stream, 1, (size_t)size, f) != (size_t)size) {
        return 1;
    }
    sixel_set_decoder_threads(threads);
    sixel_set_encoder_threads(threads);
    for (i = 0; i < runs; i++) {
        free(pixels);
        free(palette);
        t = now();
        if (SIXEL_FAILED(sixel_decode_raw(stream, (int)size, &pixels, &width,
                                          &height, &palette, &ncolors,
                                          NULL))) {
            return 2;
        }
        t = now() - t;
        dec = t < dec ? t : dec;
    }
    copy = malloc((size_t)width * (size_t)height);
    for (i = 0; i < runs; i++) {
        sixel_dither_t *dither;
        sixel_output_t *output;

        memcpy(copy, pixels, (size_t)width * (size_t)height);
        sixel_dither_new(&dither, ncolors, NULL);
        sixel_dither_set_palette(dither, palette);
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8);
        sixel_output_new(&output, sink, NULL, NULL);
        sixel_output_set_encode_policy(output, SIXEL_ENCODEPOLICY_FAST);
        bytes = 0;
        t = now();
        if (SIXEL_FAILED(sixel_encode(copy, width, height, 1, dither,
                                      output))) {
            return 3;
        }
        t = now() - t;
        enc = t < enc ? t : enc;
        sixel_output_unref(output);
        sixel_dither_unref(dither);
    }
    printf("%d colors, %zu bytes: encode %.3f ms, decode %.3f ms\n", ncolors,
           bytes, enc * 1e3, dec * 1e3);
    return 0;
}
"""


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    """Run a command, failing on a nonzero status."""
    return subprocess.run(command, check=True, text=True,
                          capture_output=True, **kwargs)


def load_average() -> float:
    """Return the one-minute load average."""
    return round(os.getloadavg()[0], 2)


def patched(source: str, replacements: list[tuple[str, str]]) -> str:
    """Apply text replacements, each of which must match."""
    for old, new in replacements:
        if old not in source:
            raise SystemExit(f"measurement patch no longer applies: {old!r}")
        source = source.replace(old, new, 1)
    return source


def build(code: Path, step: str, replacements: list[tuple[str, str]],
          name: str, cflags: str, cc: str) -> Path:
    """Compile a patched copy of one step, with a timer header."""
    source = (code / f"{step}.c").read_text(encoding="utf-8")
    source = TIMER_HEADER + patched(source, replacements)
    path = code / f"{name}.c"
    path.write_text(source, encoding="utf-8")
    binary = code / name
    run([cc, *cflags.split(), "-o", str(binary), str(code / "common.c"),
         str(path), "-lpthread"], cwd=code)
    return binary


def parse_bench(line: str) -> dict[str, float | int]:
    """Parse one `stepN bench` result."""
    match = BENCH_RE.search(line)
    if match is None:
        raise SystemExit(f"unexpected bench output: {line!r}")
    return {
        "colors": int(match["colors"]),
        "bytes": int(match["bytes"]),
        "encode_ms": float(match["enc"]),
        "decode_ms": float(match["dec"]),
    }


def faster(store: dict, key: str, value: dict) -> None:
    """Keep the fastest encode and decode seen for a key."""
    if key not in store:
        store[key] = dict(value)
        return
    for field in ("encode_ms", "decode_ms"):
        store[key][field] = min(store[key][field], value[field])


def min_lines(stderr: str, prefix: str) -> dict[str, float]:
    """Return the smallest value per name in `prefix name=value` lines."""
    best: dict[str, float] = {}
    for line in stderr.splitlines():
        if not line.startswith(prefix + " "):
            continue
        for field in line.split()[1:]:
            name, value = field.split("=")
            best[name] = min(best.get(name, 1e30), float(value))
    return {name: round(value, 3) for name, value in best.items()}


def measure_bench(out: Path, rounds: int, cc: str, cflags: str) -> dict:
    """Run `make bench` and keep the fastest of the rounds."""
    bench: dict[str, dict] = {}
    for _ in range(rounds):
        result = run(["make", "-s", f"O={out}", f"CC={cc}",
                      f"CFLAGS={cflags}", "bench"], cwd=CODE)
        for line in result.stdout.splitlines():
            step, name, rest = line.split(" ", 2)
            faster(bench.setdefault(step, {}), name, parse_bench(rest))
    return bench


def timer_output(binary: Path, picture: Path, prefix: str) -> dict:
    """Run a patched program's bench and collect its timer lines."""
    result = run([str(binary), "bench", str(picture), "1", "10"])
    return min_lines(result.stderr, prefix)


def measure_phases(code: Path, out: Path, cc: str, cflags: str) -> dict:
    """Time the encoder phases of steps 2 to 5."""
    report = 'fprintf(stderr, "phases classify=%.3f compose=%.3f emit=%.3f\\n",\
 pt_sum[0] * 1e3, pt_sum[1] * 1e3, pt_sum[2] * 1e3);\
 pt_sum[0] = pt_sum[1] = pt_sum[2] = 0;'
    begin = "{ double pt_t = pt_now();"
    def lap(i: int) -> str:
        return f"pt_sum[{i}] += pt_now() - pt_t; pt_t = pt_now();"
    flat = [
        ("        /* 1. classify:", f"        {begin} /* 1. classify:"),
        ("        /* 2. compose:", f"        {lap(0)} /* 2. compose:"),
    ]
    patches = {
        "step2": flat + [
            ("        /* 3. order:", f"        {lap(1)} /* 3. order:"),
            ("        put_band(out, list);\n",
             f"        put_band(out, list); {lap(2)} }}\n"),
            ("    free(map);\n", f"    {report}\n    free(map);\n"),
        ],
        "step3": flat + [
            ("        /* 3. emit,", f"        {lap(1)} /* 3. emit,"),
            ("        put_band(out, heads, starts, words, w, nnodes);\n",
             "        put_band(out, heads, starts, words, w, nnodes); "
             f"{lap(2)} }}\n"),
            ("    free(map);\n", f"    {report}\n    free(map);\n"),
        ],
    }
    band = [
        ("    compose(e);\n", f"    {lap(0)} compose(e); {lap(1)}\n"),
        ("    put_band(e, out);\n", f"    put_band(e, out); {lap(2)} }}\n"),
        ("    encoder_free(&e);\n    return 0;",
         f"    encoder_free(&e);\n    {report}\n    return 0;"),
    ]
    patches["step4"] = [("    load_columns(e, img, y, rows);\n",
                         f"    {begin} load_columns(e, img, y, rows);\n")] + band
    patches["step5"] = [("    from = e->width & ~15;\n",
                         f"    {begin} from = e->width & ~15;\n")]
    patches["step5"] = [("#ifdef HAVE_SIMD\n    from = e->width & ~15;\n",
                         f"    {begin}\n#ifdef HAVE_SIMD\n"
                         "    from = e->width & ~15;\n")] + band
    phases: dict[str, dict] = {}
    for step, replacements in patches.items():
        binary = build(code, step, replacements, f"phases-{step}", cflags, cc)
        phases[step] = {}
        for name, _ in INPUTS[1:]:
            phases[step][name] = timer_output(binary, out / f"{name}.ppm",
                                              "phases")
    return phases


def measure_materialize(code: Path, out: Path, cc: str, cflags: str) -> dict:
    """Time the pass from column words to rows in steps 4 and 5."""
    replacements = [(
        "    materialize(&d, img, 0, (d.used_height + 5) / 6);\n",
        "    { double pt_t = pt_now();\n"
        "    materialize(&d, img, 0, (d.used_height + 5) / 6);\n"
        '    fprintf(stderr, "materialize ms=%.3f\\n",'
        " (pt_now() - pt_t) * 1e3); }\n",
    )]
    result: dict[str, dict] = {}
    for step in ("step4", "step5"):
        binary = build(code, step, replacements, f"mat-{step}", cflags, cc)
        result[step] = {}
        for name, _ in INPUTS[1:]:
            result[step][name] = timer_output(binary, out / f"{name}.ppm",
                                              "materialize")["ms"]
    return result


def measure_prescan(code: Path, out: Path, cc: str, cflags: str) -> dict:
    """Time the serial prescan of step 6."""
    replacements = [(
        "    if (prescan(body, stop, &ps) != 0) {\n",
        "    double pt_t = pt_now();\n"
        "    int pt_rc = prescan(body, stop, &ps);\n"
        '    fprintf(stderr, "prescan ms=%.3f\\n",'
        " (pt_now() - pt_t) * 1e3);\n"
        "    if (pt_rc != 0) {\n",
    )]
    binary = build(code, "step6", replacements, "prescan", cflags, cc)
    return {name: timer_output(binary, out / f"{name}.ppm", "prescan")["ms"]
            for name, _ in INPUTS[1:]}


def measure_attribution(code: Path, out: Path, cc: str, cflags: str) -> dict:
    """Decode with one technique of step 3 at a time."""
    masked_store = (
        "                    dst = cv.pixels + (size_t)(y + r) * cv.width + x;\n"
        "                    if (repeat == 1) {\n"
        "                        *dst = (unsigned char)color;"
        "    /* the common case */\n"
        "                    } else {\n"
        "                        memset(dst, color, (size_t)repeat);\n"
        "                    }\n",
        "                    dst = cv.pixels + (size_t)(y + r) * cv.width + x;\n"
        "                    memset(dst, color, (size_t)repeat);\n",
    )
    bit_loop = (
        "                for (r = 0; r < 6; r++) {\n"
        "                    if ((bits >> r & 1) == 0) {\n"
        "                        continue;\n"
        "                    }\n",
        "                if (repeat == 1) {\n"
        "                    dst = cv.pixels + (size_t)y * cv.width + x;\n"
        "                    for (r = 0; r < 6; r++, dst += cv.width) {\n"
        "                        unsigned int m = 0u - "
        "((unsigned int)bits >> r & 1u);\n"
        "                        *dst = (unsigned char)((*dst & ~m) |\n"
        "                            ((unsigned int)color & m));\n"
        "                    }\n"
        "                } else for (r = 0; r < 6; r++) {\n"
        "                    if ((bits >> r & 1) == 0) {\n"
        "                        continue;\n"
        "                    }\n",
    )
    branchy = (
        "                    mask = 0u - ((unsigned int)bits >> r & 1u);\n"
        "                    *dst = (unsigned char)((*dst & ~mask) |\n"
        "                                           ((unsigned int)color"
        " & mask));\n",
        "                    if (bits >> r & 1) {\n"
        "                        *dst = (unsigned char)color;\n"
        "                    }\n"
        "                    (void)mask;\n",
    )
    variants = {
        "masks only": ("step2", [masked_store, bit_loop]),
        "inner loop only": ("step3", [branchy]),
    }
    report: dict[str, dict] = {}
    for label, (step, replacements) in variants.items():
        name = "attr-" + label.replace(" ", "-")
        binary = build(code, step, replacements, name, cflags, cc)
        report[label] = {}
        for picture, _ in INPUTS[1:]:
            best = 1e30
            for _ in range(2):
                result = run([str(binary), "bench", str(out / f"{picture}.ppm"),
                              "1", "10"])
                best = min(best, parse_bench(result.stdout)["decode_ms"])
            report[label][picture] = best
    return report


def measure_threads(out: Path) -> dict:
    """Sweep the thread count of step 6."""
    sweep: dict[str, dict] = {}
    for threads in (1, 2, 4, 8, 10, 14):
        env = dict(os.environ, TUTORIAL_THREADS=str(threads))
        sweep[str(threads)] = {}
        for name, _ in INPUTS[1:]:
            result = run([str(out / "step6"), "bench",
                          str(out / f"{name}.ppm"), "1", "15"], env=env)
            sweep[str(threads)][name] = parse_bench(result.stdout)
    return sweep


def measure_libsixel(build_dir: Path, source_dir: Path, revision: str,
                     out: Path, cc: str) -> dict:
    """Run the same pictures through a libsixel build."""
    bench = out / "libsixel-bench"
    source = out / "libsixel-bench.c"
    source.write_text(LIBSIXEL_BENCH, encoding="utf-8")
    run([cc, "-O2", f"-I{build_dir}/include", f"-I{source_dir}/include",
         "-o", str(bench), str(source), f"-L{build_dir}/src", "-lsixel",
         f"-Wl,-rpath,{build_dir}/src"])
    report: dict[str, object] = {"revision": revision}
    for name, _ in INPUTS[1:]:
        stream = out / f"{name}.six"
        stream.write_text("", encoding="utf-8")
        with stream.open("wb") as handle:
            subprocess.run([str(out / "step2"), "encode",
                            str(out / f"{name}.ppm")], check=True,
                           stdout=handle)
        report[name] = {}
        for threads in (1, 14):
            best: dict[str, dict] = {}
            for _ in range(2):
                result = run([str(bench), str(stream), str(threads), "10"])
                faster(best, "x", parse_bench(result.stdout))
            report[name][str(threads)] = best["x"]
    return report


def environment(cc: str, cflags: str, before: float, after: float) -> dict:
    """Record what the numbers depend on."""
    def output(*command: str) -> str:
        return run(list(command), cwd=ROOT).stdout.strip()
    machine = platform.machine()
    try:
        machine = output("sysctl", "-n", "machdep.cpu.brand_string")
    except (OSError, subprocess.CalledProcessError):
        pass
    return {
        "date": datetime.date.today().isoformat(),
        "revision": output("git", "rev-parse", "--short", "HEAD"),
        "machine": machine,
        "system": f"{platform.system()} {platform.release()}",
        "compiler": output(cc, "--version").splitlines()[0],
        "cflags": cflags,
        "load_before": before,
        "load_after": after,
    }


def keep_fastest(previous: object, current: object) -> object:
    """Merge two result trees, keeping the smaller of each timing."""
    if isinstance(previous, dict) and isinstance(current, dict):
        return {key: keep_fastest(previous[key], value)
                if key in previous else value
                for key, value in current.items()}
    if isinstance(previous, float) and isinstance(current, float):
        return min(previous, current)
    return current


def measure(arguments: argparse.Namespace) -> None:
    """Run every benchmark and merge the timings into the results file."""
    cc = "cc"
    cflags = "-O2 -Wall -Wextra"
    before = load_average()
    with tempfile.TemporaryDirectory(prefix="sixel-tutorial-") as temp:
        out = Path(temp) / "bin"
        out.mkdir()
        code = Path(temp) / "code"
        shutil.copytree(CODE, code)
        results = {"bench": measure_bench(out, arguments.rounds, cc, cflags)}
        results["attribution"] = measure_attribution(code, out, cc, cflags)
        results["phases"] = measure_phases(code, out, cc, cflags)
        results["materialize"] = measure_materialize(code, out, cc, cflags)
        results["prescan"] = measure_prescan(code, out, cc, cflags)
        results["threads"] = measure_threads(out)
        if arguments.libsixel_build:
            results["libsixel"] = measure_libsixel(
                arguments.libsixel_build, arguments.libsixel_source,
                arguments.libsixel_revision, out, cc)
        results["environment"] = environment(cc, cflags, before,
                                             load_average())
    RESULTS.parent.mkdir(exist_ok=True)
    if RESULTS.exists() and not arguments.fresh:
        previous = json.loads(RESULTS.read_text(encoding="utf-8"))
        if previous["environment"]["revision"] == \
                results["environment"]["revision"]:
            environment_now = results.pop("environment")
            results = keep_fastest(previous, results)
            results["environment"] = environment_now
    RESULTS.write_text(json.dumps(results, indent=2, sort_keys=True) + "\n",
                       encoding="utf-8")


def ms(value: float) -> str:
    """Format milliseconds with about three significant digits."""
    return f"{value:.1f}" if value >= 10 else f"{value:.2f}"


def ratio(before: float, after: float) -> str:
    """Format a speed-up."""
    value = before / after
    return f"{value:.0f}×" if value >= 10 else f"{value:.1f}×"


def table(header: list[str], rows: list[list[str]]) -> str:
    """Format a Markdown table."""
    lines = ["| " + " | ".join(header) + " |",
             "| " + " | ".join("---" for _ in header) + " |"]
    lines += ["| " + " | ".join(row) + " |" for row in rows]
    return "\n".join(lines)


def render_summary(results: dict) -> str:
    """The table of every step and input."""
    rows = []
    for number in range(1, 7):
        cells = results["bench"][f"step{number}"]
        rows.append([str(number)] + [
            f"{ms(cells[name]['encode_ms'])} / {ms(cells[name]['decode_ms'])}"
            for name, _ in INPUTS])
    return table(["Step"] + [label for _, label in INPUTS], rows)


def render_step(results: dict, number: int) -> str:
    """One step's table, compared with the step before it."""
    cells = results["bench"][f"step{number}"]
    if number == 1:
        return table(["Input", "Encode", "Decode", "Stream"], [
            [label, f"{ms(cells[name]['encode_ms'])} ms",
             f"{ms(cells[name]['decode_ms'])} ms",
             f"{cells[name]['bytes']:,} bytes"] for name, label in INPUTS])
    previous = results["bench"][f"step{number - 1}"]
    return table(
        ["Input", "Encode", "Speed-up", "Decode", "Speed-up", "Stream"], [
            [label, f"{ms(cells[name]['encode_ms'])} ms",
             ratio(previous[name]["encode_ms"], cells[name]["encode_ms"]),
             f"{ms(cells[name]['decode_ms'])} ms",
             ratio(previous[name]["decode_ms"], cells[name]["decode_ms"]),
             f"{cells[name]['bytes']:,} bytes"] for name, label in INPUTS])


def render_environment(results: dict) -> str:
    """The conditions of the measurement."""
    env = results["environment"]
    return table(["Item", "Value"], [
        ["Date", env["date"]],
        ["Based on revision", f"`{env['revision']}`"],
        ["Machine", env["machine"]],
        ["System", env["system"]],
        ["Compiler", env["compiler"]],
        ["Flags", f"`{env['cflags']}`"],
        ["Load average before, after",
         f"{env['load_before']}, {env['load_after']}"],
    ])


def render_attribution(results: dict) -> str:
    """Decode times with step 3's techniques applied one at a time."""
    rows = []
    for label, step in (("Step 2", "step2"), ("Step 3", "step3")):
        rows.append([label] + [ms(results["bench"][step][name]["decode_ms"])
                               for name, _ in INPUTS[1:]])
    for label, key in (("Step 2 with masks for single characters",
                        "masks only"),
                       ("Step 3 without the masks", "inner loop only")):
        rows.insert(-1, [label] + [ms(results["attribution"][key][name])
                                   for name, _ in INPUTS[1:]])
    return table(["Decoder"] + [label for _, label in INPUTS[1:]], rows)


def render_phases(results: dict) -> str:
    """Encoder phases per step and input."""
    rows = []
    for step in ("step2", "step3", "step4", "step5"):
        for name, label in INPUTS[1:]:
            cell = results["phases"][step][name]
            rows.append([f"{step[-1]}", label, ms(cell["classify"]),
                         ms(cell["compose"]), ms(cell["emit"])])
    return table(["Step", "Input", "Classify", "Cut nodes", "Emit"], rows)


def render_materialize(results: dict) -> str:
    """The pass from column words to rows."""
    return table(["Step"] + [label for _, label in INPUTS[1:]], [
        [step[-1]] + [ms(results["materialize"][step][name])
                      for name, _ in INPUTS[1:]]
        for step in ("step4", "step5")])


def render_prescan(results: dict) -> str:
    """The serial prescan against the whole threaded decode."""
    rows = []
    for name, label in INPUTS[1:]:
        whole = results["bench"]["step6"][name]["decode_ms"]
        rows.append([label, ms(results["prescan"][name]), ms(whole)])
    return table(["Input", "Prescan", "Whole decode"], rows)


def render_threads(results: dict) -> str:
    """Thread sweep of step 6."""
    rows = []
    base = results["threads"]["1"]
    for threads in sorted(results["threads"], key=int):
        cells = results["threads"][threads]
        row = [threads]
        for name, _ in INPUTS[1:]:
            cell = cells[name]
            row.append(
                f"{ms(cell['encode_ms'])} "
                f"({ratio(base[name]['encode_ms'], cell['encode_ms'])}) / "
                f"{ms(cell['decode_ms'])} "
                f"({ratio(base[name]['decode_ms'], cell['decode_ms'])})")
        rows.append(row)
    return table(["Threads"] + [label for _, label in INPUTS[1:]], rows)


def render_libsixel(results: dict) -> str:
    """The tutorial against libsixel on the same indexed pictures."""
    rows = []
    report = results["libsixel"]
    for name, label in INPUTS[1:]:
        serial = results["bench"]["step5"][name]
        threaded = results["threads"]["14"][name]
        rows.append([label, "1", ms(serial["encode_ms"]),
                     ms(report[name]["1"]["encode_ms"]),
                     ms(serial["decode_ms"]),
                     ms(report[name]["1"]["decode_ms"])])
        rows.append([label, "14", ms(threaded["encode_ms"]),
                     ms(report[name]["14"]["encode_ms"]),
                     ms(threaded["decode_ms"]),
                     ms(report[name]["14"]["decode_ms"])])
    return table(["Input", "Threads", "Encode, tutorial", "Encode, libsixel",
                  "Decode, tutorial", "Decode, libsixel"], rows)


def render(results: dict, block: str) -> str:
    """Return the Markdown of one measurement block."""
    if block.startswith("step"):
        return render_step(results, int(block[-1]))
    return {
        "summary": render_summary,
        "environment": render_environment,
        "attribution": render_attribution,
        "phases": render_phases,
        "materialize": render_materialize,
        "prescan": render_prescan,
        "threads": render_threads,
        "libsixel": render_libsixel,
    }[block](results)


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
            pattern = re.escape(begin) + r".*?" + re.escape(end)
            content = f"{begin}\n{render(results, block)}\n{end}"
            updated = re.sub(pattern, lambda _: content, updated, count=1,
                             flags=re.S)
        if updated != source:
            stale.append(page)
            if not check:
                path.write_text(updated, encoding="utf-8")
    if check and stale:
        raise SystemExit("stale measurement tables: " + ", ".join(stale))


def parse_args() -> argparse.Namespace:
    """Parse the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--measure", action="store_true")
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--fresh", action="store_true",
                        help="discard earlier timings of this revision")
    parser.add_argument("--libsixel-build", type=Path,
                        help="a Meson build directory of libsixel")
    parser.add_argument("--libsixel-source", type=Path, default=ROOT)
    parser.add_argument("--libsixel-revision", default="unknown")
    parser.add_argument("--no-pages", action="store_true",
                        help="with --measure, leave the pages alone")
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def main() -> None:
    """Measure and render as requested."""
    arguments = parse_args()
    if arguments.measure:
        measure(arguments)
    if not (arguments.measure and arguments.no_pages):
        update_pages(arguments.check)


if __name__ == "__main__":
    main()
