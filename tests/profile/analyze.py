"""Join private Nimf spans to validated, serial input samples."""

import argparse
from bisect import bisect_right
from collections import defaultdict
import csv
from dataclasses import dataclass, field
import json
import math
from pathlib import Path


@dataclass
class Span:
    pid: int
    thread: int
    id: int
    parent: int
    name: str
    detail: int
    start: int
    end: int
    cpu_start: int
    cpu_end: int
    cpu_enabled: bool
    children: list["Span"] = field(default_factory=list)

    @property
    def wall(self) -> int:
        return self.end - self.start

    @property
    def self_wall(self) -> int:
        return self.wall - sum(child.wall for child in self.children)

    @property
    def cpu(self) -> int:
        return self.cpu_end - self.cpu_start

    @property
    def self_cpu(self) -> int:
        return self.cpu - sum(child.cpu for child in self.children)


def read_spans(directory: Path) -> list[Span]:
    spans = []
    files = sorted(directory.glob("nimf-*.csv"))
    if not files:
        raise ValueError("No traces: build with --enable-profiling and set NIMF_PROFILE_DIR")
    for path in files:
        with path.open() as stream:
            header = stream.readline().strip().split()
            if header[:2] != ["#", "nimf-profile-v1"]:
                raise ValueError(f"{path}: missing completion header; process may have been killed")
            metadata = dict(item.split("=", 1) for item in header[2:])
            if any(int(metadata[key]) for key in ("dropped", "incomplete", "clock_errors")):
                raise ValueError(f"{path}: incomplete trace: {metadata}")
            records = {}
            for row in csv.DictReader(stream):
                span = Span(int(metadata["pid"]), int(metadata["thread"]),
                            int(row["id"]), int(row["parent"]), row["span"],
                            int(row["detail"]), int(row["start_ns"]), int(row["end_ns"]),
                            int(row["cpu_start_ns"]), int(row["cpu_end_ns"]),
                            metadata["cpu"] == "1")
                if span.id in records or span.start <= 0 or span.end < span.start or span.cpu < 0:
                    raise ValueError(f"{path}: invalid span {span.id}")
                if span.parent:
                    parent = records.get(span.parent)
                    if parent is None or not parent.start <= span.start <= span.end <= parent.end:
                        raise ValueError(f"{path}: invalid parent of {span.id}")
                    if parent.children and parent.children[-1].end > span.start:
                        raise ValueError(f"{path}: overlapping siblings at {span.id}")
                    parent.children.append(span)
                records[span.id] = span
            if len(records) != int(metadata["records"]):
                raise ValueError(f"{path}: truncated trace")
            for span in records.values():
                if span.self_wall < 0 or span.self_cpu < 0:
                    raise ValueError(f"{path}: negative exclusive time at {span.id}")
            spans.extend(records.values())
    return spans


def descendants(span: Span) -> list[Span]:
    return [span] + [item for child in span.children for item in descendants(child)]


def quantile(values: list[float], percentile: int) -> float:
    ordered = sorted(values)
    return ordered[math.ceil(len(ordered) * percentile / 100) - 1]


def analyze(directory: Path) -> None:
    spans = read_spans(directory / "profile")
    with (directory / "samples.csv").open() as stream:
        samples = list(csv.DictReader(stream))
    if not samples:
        raise ValueError("No accepted input samples")
    starts = [int(sample["injected_us"]) * 1000 for sample in samples]
    if any(left >= right for left, right in zip(starts, starts[1:])):
        raise ValueError("Input windows must be serial and ordered")
    # The observer requires 25 ms of stability after its final pixel timestamp.
    limits = starts[1:] + [(int(samples[-1]["screen_upper_us"]) + 25000) * 1000]
    roots = defaultdict(list)
    for span in spans:
        if span.name not in ("client.filter", "server.filter") or span.detail != 0:
            continue
        if span.end <= starts[0] or span.start >= limits[-1]:
            continue
        index = bisect_right(starts, span.start) - 1
        if index < 0 or span.end > limits[index]:
            raise ValueError(f"{span.name} span {span.id} crosses an input window")
        roots[index, span.name].append(span)

    groups = defaultdict(list)
    keys = []
    events = []
    selected = set()
    for index, sample in enumerate(samples):
        paired = {}
        for role in ("client", "server"):
            candidates = sorted(roots[index, f"{role}.filter"], key=lambda span: span.start)
            if not candidates or len({span.pid for span in candidates}) != 1:
                raise ValueError(f"Sample {index}: expected one {role} process with key presses")
            if any(left.end > right.start for left, right in zip(candidates, candidates[1:])):
                raise ValueError(f"Sample {index}: overlapping {role} filters")
            paired[role] = candidates
            for span in [item for root in candidates for item in descendants(root)]:
                identity = span.pid, span.thread, span.id
                if identity in selected:
                    raise ValueError("A span belongs to multiple input samples")
                selected.add(identity)
                group = (sample["phase"], sample["step"], role, span.name, span.detail)
                groups[group].append(span)
                events.append({"name": span.name, "ph": "X", "pid": span.pid,
                               "tid": span.thread, "ts": span.start / 1000,
                               "dur": span.wall / 1000,
                               "args": {"detail": span.detail, "phase": sample["phase"],
                                        "step": sample["step"], "cycle": int(sample["cycle"]),
                                        "self_wall_us": span.self_wall / 1000}})
        clients, servers = paired["client"], paired["server"]
        if len(clients) != len(servers) or any(
                client.pid == server.pid or not client.start <= server.start <= server.end <= client.end
                for client, server in zip(clients, servers)):
            raise ValueError(f"Sample {index}: client/server intervals do not match")
        client_wall = sum(span.wall for span in clients)
        server_wall = sum(span.wall for span in servers)
        keys.append({"phase": sample["phase"], "step": sample["step"], "cycle": sample["cycle"],
                     "filter_calls": len(clients),
                     "client_us": client_wall / 1000, "server_us": server_wall / 1000,
                     "client_span_us": (clients[-1].end - clients[0].start) / 1000,
                     "outside_server_us": (client_wall - server_wall) / 1000,
                     "injection_to_client_us": (clients[0].start - starts[index]) / 1000,
                     "injection_to_model_us": float(sample["model_ms"]) * 1000})

    summaries = []
    for group, records in sorted(groups.items()):
        row = dict(zip(("phase", "step", "role", "span", "detail"), group))
        row["calls"] = len(records)
        for name in ("wall", "self_wall", "cpu", "self_cpu"):
            values = [getattr(span, name) / 1000 for span in records]
            enabled = "cpu" not in name or all(span.cpu_enabled for span in records)
            row[f"{name}_total_us"] = sum(values) if enabled else None
            for percentile in (50, 95, 99):
                row[f"{name}_p{percentile}_us"] = quantile(values, percentile) if enabled else None
        summaries.append(row)
    (directory / "profile-summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    (directory / "profile-trace.json").write_text(json.dumps({"traceEvents": events}) + "\n")
    with (directory / "profile-keys.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(keys[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(keys)

    lines = ["# Input-path profile", "", "All times are microseconds. Only measured key presses enter these tables.", "",
             "| Step | Keys | Filter calls | Client P50 / P95 | Server P50 / P95 | Outside server P50 |", "|---|---:|---:|---:|---:|---:|"]
    for step in sorted({row["step"] for row in keys if row["phase"] == "measured"}):
        rows = [row for row in keys if row["phase"] == "measured" and row["step"] == step]
        pair = lambda name: " / ".join(f"{quantile([row[name] for row in rows], p):.2f}" for p in (50, 95))
        lines.append(f"| {step} | {len(rows)} | {sum(row['filter_calls'] for row in rows)} | {pair('client_us')} | {pair('server_us')} | "
                     f"{quantile([row['outside_server_us'] for row in rows], 50):.2f} |")
    lines += ["", "Exclusive wall time excludes instrumented children. It includes uninstrumented work and waits.",
              "CPU time is available only with NIMF_PROFILE_CPU=1. Do not sum overlapping client/server intervals.", "",
              "| Role | Span | Detail | Calls | Total self wall | Self P50 / P95 | Total self CPU |",
              "|---|---|---:|---:|---:|---:|---:|"]
    measured = defaultdict(list)
    for (phase, step, role, name, detail), records in groups.items():
        if phase == "measured":
            measured[role, name, detail].extend(records)
    for (role, name, detail), records in sorted(measured.items(), key=lambda item: -sum(r.self_wall for r in item[1])):
        values = [span.self_wall / 1000 for span in records]
        cpu = f"{sum(span.self_cpu for span in records) / 1000:.2f}" if all(span.cpu_enabled for span in records) else "unavailable"
        lines.append(f"| {role} | {name} | {detail} | {len(records)} | {sum(values):.2f} | "
                     f"{quantile(values, 50):.2f} / {quantile(values, 95):.2f} | {cpu} |")
    (directory / "profile.md").write_text("\n".join(lines) + "\n")
    print(f"Profile validated: {len(keys)} keys, {len(events)} spans; {directory / 'profile.md'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Toolkit result directory containing samples.csv and profile/")
    arguments = parser.parse_args()
    try:
        analyze(arguments.directory)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"Invalid profile: {error}\n")
