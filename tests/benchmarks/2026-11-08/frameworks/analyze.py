import csv
import math
import statistics
from pathlib import Path

root = Path(__file__).resolve().parent
metrics = ("model_ms", "screen_lower_ms", "screen_upper_ms", "interval_ms", "readback_ms")
summaries = []
total = 0
controls = 0
shutdowns = []


def percentile(values, percent):
    values = sorted(values)
    return values[math.ceil(len(values) * percent / 100) - 1]


for directory in sorted(root.glob("trial[123]-*")):
    trial, distro, framework, language = directory.name.split("-")
    assert (directory / "status").read_text().strip() == "0", directory
    runtime = (directory / "runtime.log").read_text()
    toolkits = ("gtk3",) if (distro, framework, language) == ("ubuntu", "ibus", "ja") else ("gtk3", "gtk4")
    assert runtime.count("PASS: semantic input without pixels is rejected") == len(toolkits), directory
    assert runtime.count("PASS: visible output from the wrong input context is rejected") == len(toolkits), directory
    controls += 2 * len(toolkits)
    for toolkit in toolkits:
        shutdown_file = directory / toolkit / "shutdown.txt"
        shutdown = shutdown_file.read_text().strip() if shutdown_file.exists() else "graceful-before-bounded-cleanup"
        assert shutdown in ("graceful", "forced", "graceful-before-bounded-cleanup")
        shutdowns.append((directory.name, toolkit, shutdown))
        with (directory / toolkit / "samples.csv").open() as handle:
            rows = list(csv.DictReader(handle))
        steps = ("preedit", "commit", "erase") if language == "ja" else (
            "preedit-initial", "preedit-vowel", "preedit-final", "commit", "erase")
        assert len(rows) == 61 * len(steps), (directory, toolkit, len(rows))
        for row in rows:
            assert row["language"] == language
            injected, model, lower, upper = (int(row[key]) for key in (
                "injected_us", "model_us", "screen_lower_us", "screen_upper_us"))
            assert injected <= model <= upper
            assert injected <= lower <= upper
            key = int(row["key_received_us"])
            assert (key == 0 and row["key_ms"] == "") or (key >= injected and float(row["key_ms"]) >= 0)
        measured = [row for row in rows if row["phase"] == "measured"]
        assert len(measured) == 50 * len(steps)
        total += len(measured)
        for step in (*steps, "composition"):
            subset = [row for row in measured if row["step"].startswith("preedit")]
            if step != "composition":
                subset = [row for row in measured if row["step"] == step]
            record = dict(trial=trial, distro=distro, framework=framework,
                          language=language, toolkit=toolkit, step=step, n=len(subset))
            for metric in metrics:
                values = [float(row[metric]) for row in subset]
                for percent in (50, 95, 99, 100):
                    record[f"{metric}_p{percent}"] = percentile(values, percent)
            first = next(row for row in rows if row["phase"] == "first" and row["step"] == steps[0])
            record["first_screen_ms"] = float(first["screen_upper_ms"])
            summaries.append(record)

with (root / "summary.csv").open("w") as handle:
    writer = csv.DictWriter(handle, fieldnames=list(summaries[0]), lineterminator="\n")
    writer.writeheader()
    writer.writerows(summaries)

lines = [f"Measured keys: {total}; negative controls: {controls}", "",
         "Per-run composition percentiles, median across 3 runs (ms). Korean pools three composition stages equally.", "",
         "| OS | GTK | Language | Framework | P50 | P50 range | P95 | Model P50 | Interval P50 | First input range |",
         "|---|---|---|---|---:|---|---:|---:|---:|---|"]
for distro in ("ubuntu", "arch"):
    for toolkit in ("gtk3", "gtk4"):
        for language in ("ja", "ko"):
            frameworks = ("nimf", "ibus", "fcitx5", "upstream") if language == "ko" else ("nimf", "ibus", "fcitx5")
            for framework in frameworks:
                if (distro, toolkit, language, framework) == ("ubuntu", "gtk4", "ja", "ibus"):
                    lines.append("| ubuntu | gtk4 | ja | ibus | invalid | preedit remains after commit | — | — | — | — |")
                    continue
                group = [row for row in summaries if row["distro"] == distro and row["toolkit"] == toolkit
                         and row["language"] == language and row["framework"] == framework and row["step"] == "composition"]
                assert len(group) == 3, (distro, toolkit, language, framework)
                def med(key):
                    return statistics.median(row[key] for row in group)
                def span(key):
                    values = [row[key] for row in group]
                    return f"{min(values):.3f}–{max(values):.3f}"
                lines.append(f"| {distro} | {toolkit} | {language} | {framework} | {med('screen_upper_ms_p50'):.3f} | "
                             f"{span('screen_upper_ms_p50')} | {med('screen_upper_ms_p95'):.3f} | {med('model_ms_p50'):.3f} | "
                             f"{med('interval_ms_p50'):.3f} | {span('first_screen_ms')} |")
(root / "tables.md").write_text("\n".join(lines) + "\n")
with (root / "shutdowns.csv").open("w") as handle:
    writer = csv.writer(handle, lineterminator="\n")
    writer.writerow(("case", "toolkit", "shutdown"))
    writer.writerows(shutdowns)
compact = ["Composition input-to-screen upper bounds in ms. Each cell is median P50 / median P95 across three runs.", ""]
for language in ("ko", "ja"):
    compact.extend([f"## {'Korean' if language == 'ko' else 'Japanese'}", "",
                    "| OS / GTK | Nimf fork | Upstream Nimf | IBus | Fcitx5 |", "|---|---:|---:|---:|---:|"])
    for distro in ("ubuntu", "arch"):
        for toolkit in ("gtk3", "gtk4"):
            cells = [f"{distro} / {toolkit}"]
            for framework in ("nimf", "upstream", "ibus", "fcitx5"):
                group = [r for r in summaries if (r["distro"], r["toolkit"], r["language"], r["framework"], r["step"])
                         == (distro, toolkit, language, framework, "composition")]
                if not group:
                    cells.append("unsupported Mozc" if framework == "upstream" else "correctness failure")
                else:
                    p50 = statistics.median(r["screen_upper_ms_p50"] for r in group)
                    p95 = statistics.median(r["screen_upper_ms_p95"] for r in group)
                    cells.append(f"{p50:.2f} / {p95:.2f}")
            compact.append("| " + " | ".join(cells) + " |")
    compact.append("")
(root / "comparison.md").write_text("\n".join(compact).rstrip() + "\n")
print("\n".join(lines))
print("Forced shutdowns:", sum(r[2] == "forced" for r in shutdowns), "of", len(shutdowns))
