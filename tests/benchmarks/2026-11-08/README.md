# Input benchmarks, 2026-11-08

Measurements used isolated Docker/Xvfb sessions on Ubuntu 24.04 and Arch updated
on the measurement date. They measure framebuffer observation, not physical
display latency. The host was shared and CPU affinity was not pinned.

| Experiment | Report | Accepted work |
|---|---|---|
| Forked Nimf, upstream Nimf, IBus, Fcitx5 | [Framework comparison](frameworks/RESULTS.md) | 42 executions, 16,950 measured keys |
| Nimf `-O2` versus `-O3` | [Compiler optimization](optimization/RESULTS.md) | 12 executions, 7,200 measured keys |
| Before and after memory safety changes | [Memory footprint](footprint/RESULTS.md) | 12 executions, 120,000 verified conversions |

Korean composition P50 for the current fork was 1.27–1.33 ms. Small differences
from upstream Nimf and Fcitx5 did not establish an overall winner. O3 did not
consistently improve latency. After 10,000 Japanese conversions, current-stack
PSS was 72.86 MiB on Ubuntu and 71.01 MiB on Arch. Each report includes ranges,
startup behavior, correctness exclusions, versions, and scope limitations.

## Source identity

Measurements preceded publication. The resulting source commits are
`25d6e6b` (Mozc and the default engine profile) and `45a4126` (memory safety).
Their parent was `3dc9294a9f71499a51cfd9ca5a1a41c3baf5e647`.
The untouched upstream baseline was `hamonikr/nimf` at
`b4a141aec1c833541f163da8b1beb9ab1e0c0615`, version 1.4.19.
Only Korean has an upstream comparison because that revision has no Mozc engine.

The reports identify immutable measurement images. Source manifests describe
the files at measurement time, before these archive guides were added.
[Product-source hashes](../../memory/product-source.sha256) identify the final
memory-safety implementation. Docker images and dependency source trees are
not bundled. Rebuilding from current package mirrors creates a new experiment.

Each `raw.tar.gz` contains samples and correctness diagnostics, with a companion
SHA-256 checksum. Full Docker inspection metadata, process environment dumps,
build trees, crash dumps, and reviewer session metadata are excluded.
Framework image IDs are retained in `frameworks/images.json`.

## Regenerate the published tables

Run from the repository root with Python 3, GNU tar, and `sha256sum`:

```sh
archive="$PWD/tests/benchmarks/2026-11-08"
mkdir -p build
output=$(mktemp -d "$PWD/build/published-benchmarks-XXXXXX")
for experiment in frameworks optimization footprint; do
  (cd "$archive/$experiment" && sha256sum -c raw.sha256)
  mkdir "$output/$experiment"
  tar -xzf "$archive/$experiment/raw.tar.gz" -C "$output/$experiment"
done
cp "$archive/frameworks/analyze.py" "$output/frameworks/"
python3 "$output/frameworks/analyze.py"
python3 "$archive/optimization/summarize.py" "$output/optimization/accepted"
python3 "$archive/footprint/summarize.py" "$output/footprint"

for file in summary.csv tables.md comparison.md shutdowns.csv; do
  diff -u "$archive/frameworks/$file" "$output/frameworks/$file"
done
for file in summary.json trial-stats.json first-input.json tables.md; do
  diff -u "$archive/optimization/$file" "$output/optimization/accepted/$file"
done
for file in summary.json trial-medians.json tables.md; do
  diff -u "$archive/footprint/$file" "$output/footprint/$file"
done
```

## Run new framework measurements

First prepare each base with the [latency runner](../../latency/README.md).
That creates `nimf-latency-ubuntu:test` and `nimf-latency-arch:test` and performs
a Nimf-only validation run. Then build the comparison layers from the repo root:

```sh
archive="$PWD/tests/benchmarks/2026-11-08"
recipe="$archive/frameworks/recipe"
docker build -f "$recipe/Dockerfile.ubuntu" -t nimf-frameworks-ubuntu:deps .
docker build -f "$recipe/Dockerfile.arch" -t nimf-frameworks-arch:deps .
docker build -f "$recipe/Dockerfile.arch-mozc" -t nimf-frameworks-arch:mozc .
docker build -f "$recipe/Dockerfile.arch-fcitx" -t nimf-frameworks-arch:fcitx .
for pair in ubuntu:deps arch:mozc arch:fcitx; do
  distro=${pair%%:*}
  suffix=
  if [ "$pair" = arch:fcitx ]; then suffix=-fcitx; fi
  docker build -f "$recipe/Dockerfile.probes" \
    --build-arg "BASE_IMAGE=nimf-frameworks-$pair" \
    -t "nimf-frameworks-${distro}:probe$suffix" .
done
```

The upstream Dockerfile expects a build context containing `source/`, exported
from the upstream revision above. If that object is available locally:

```sh
context=$(mktemp -d "$PWD/build/upstream-context-XXXXXX")
mkdir "$context/source"
git archive b4a141aec1c833541f163da8b1beb9ab1e0c0615 | tar -x -C "$context/source"
for distro in ubuntu arch; do
  docker build -f "$recipe/Dockerfile.upstream" \
    --build-arg "BASE_IMAGE=nimf-frameworks-${distro}:probe" \
    -t "nimf-frameworks-${distro}:upstream" "$context"
done
export COMPARISON_RESULTS=$(mktemp -d "$PWD/build/framework-comparison-XXXXXX")
bash "$recipe/run-series.sh"
cp "$archive/frameworks/analyze.py" "$COMPARISON_RESULTS/"
python3 "$COMPARISON_RESULTS/analyze.py"
```

The historical series excludes Ubuntu IBus Japanese GTK4 after a reproduced
correctness failure. `run-case.sh` preserves that exclusion for `trial*` labels.
When changing package versions, run that case with a `preflight` label to assess
it again. Arch Fcitx5 uses a different Mozc version from Nimf and IBus; retain
that distinction when interpreting the Japanese results.

## Other measurement recipes

`optimization/recipe/Dockerfile` derives an O3 build from each retained O2
latency image. Build it with `BASE_IMAGE=nimf-latency-<distro>:test` and tag it
`nimf-latency-<distro>:o3`. Then run `optimization/recipe/run.py` with a fresh
output directory. The archived O2/O3 experiment used an earlier whole-window
pixel crop. The later framework experiment uses an inset entry crop. Keep
these datasets separate; rebuilding with the current probe changes the observer.

`footprint/recipe/Dockerfile` rebuilds the current tree over the retained
pre-safety `nimf-mozc-<distro>:test` image. Tag it
`nimf-footprint-<distro>:after`, then run `footprint/recipe/run.py` with a fresh
output directory. Its fixture directory is mounted read-only at `/probe`.
It retains stopped containers for inspection and records each name in its log.
The two baseline images must contain the distinct before/after sources;
rebuilding both from the current checkout would invalidate this comparison.
