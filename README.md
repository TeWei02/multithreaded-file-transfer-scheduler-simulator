# Multithreaded File Transfer and Scheduling Simulator

**Language:** [![中文](https://img.shields.io/badge/%E4%B8%AD%E6%96%87-%E4%B8%AD%E6%96%87%E7%89%88-blue?style=flat-square)](README.md) [![English](https://img.shields.io/badge/English-English-blue?style=flat-square)](README.en.md)

[![CI](https://github.com/TeWei02/multithreaded-file-transfer-scheduler-simulator/actions/workflows/ci.yml/badge.svg)](https://github.com/TeWei02/multithreaded-file-transfer-scheduler-simulator/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-informational)

A C++17 discrete-event simulator that models concurrent file transfer requests served by a fixed pool
of worker threads, and compares five scheduling policies on the same workload using reproducible CSV
outputs and quantitative metrics.

**Live interactive demo:** https://tewei02.github.io/multithreaded-file-transfer-scheduler-simulator/
**Equivalence check page:** https://tewei02.github.io/multithreaded-file-transfer-scheduler-simulator/verify.html

---
<div align="center">

## Demo Preview

在瀏覽器 Demo 中以 4 個 worker 執行緒驅動五種排程策略（FIFO / SJF / Priority / RR / WFQ），用 Gantt 時間軸呈現任務分派過程，並對比平均等待、平均周轉、吞吐量與 Jain 公平性指標。

<video controls width="640" src="assets/demo-preview.webm"></video>

</div>


## 1. Live interactive demo

The demo runs entirely in the browser (GitHub Pages) and lets a visitor drive the simulator without
building anything locally:

- Choose a workload: bundled datasets or an on-the-fly generated request set (adjustable request count).
- Configure the server: worker-thread count, Round Robin quantum, per-dispatch connection overhead.
- Select any subset of the five schedulers and dispatch the same workload to all of them.
- Watch the run: a Gantt-style timeline of per-thread occupancy, a playback control with step
  forward/back, a per-request detail table (arrival, start, finish, waiting, turnaround, response,
  assigned thread) and a summary table (average waiting / turnaround / response, throughput,
  Jain fairness index, total completion time).
- Compare schedulers side by side on identical input, and inspect how dispatch order changes the metrics.

The demo is installable as a PWA (`manifest.json` + `sw.js`) and keeps working offline once loaded.

**Scope of the demo — what it is not.** The page is a JavaScript re-implementation of the simulation
kernel for interactive use. It performs no real network transfers, opens no sockets and moves no bytes:
every number on the page comes from the same queuing model that the C++ core evaluates. The C++ binary
remains the reference implementation.

### 1.1 Equivalence with the C++ reference

The browser port is verified against the C++ engine rather than assumed to match it:

1. `tools/run_oracle.sh` rebuilds the C++ binary and re-runs 12 reference scenarios (varying request
   count, thread count, quantum and overhead) with all five schedulers, writing the per-request CSVs
   and summary CSVs into `oracle/` and recording their SHA-256 digests in
   `data/reference/oracle_sha256.txt`. The CSVs themselves are not committed; the digest manifest is,
   so the reference set stays auditable and `tools/run_oracle.sh --check` reproduces all 72 digests.
2. `tools/build_oracle_bundle.py` packs those reference CSVs into `docs/oracle_bundle.js`.
3. `verify.html` replays each scenario through the JavaScript kernel and compares every row and every
   summary line against the bundled reference values.

Latest verified result (12 scenarios, 2,455 request rows, 60 summary rows, 60 scheduler runs):
**0 mismatches**, maximum absolute difference **4.99e-5 s**, within the tolerances documented in
`verify.html` (2.5e-5 for values printed with four decimals, 2.5e-6 for values printed with six
decimals, allowing for the rounding the CLI applies). The check can be re-run locally with
`tools/check_demo.py` (see §9).

---

## 2. Simulated system

The simulator reproduces the following server-side scenario:

- Clients submit file transfer requests, each with an arrival time, a file size, an estimated
  bandwidth and a priority.
- The server owns a fixed number of worker threads; a worker serves one request slice at a time.
- A scheduler decides which pending request a free worker picks up next, and (for Round Robin) how
  much service each slice gets.
- Time advances in discrete events: arrival, dispatch, and completion.

The project focuses on systems fundamentals: dispatch policy behaviour, queueing delay, throughput,
fairness between clients, and reproducible benchmarking.

---

## 3. Scheduling policies

| Policy | Selection rule | Parameters |
| --- | --- | --- |
| `fifo` | First come, first served by arrival time. | — |
| `sjf` | Shortest estimated remaining transfer time first (non-preemptive). | — |
| `priority` | Highest priority first; lower priority number wins (non-preemptive). | `priority` column |
| `rr` | Round Robin: a slice is bounded by `--quantum`; unfinished requests return to the queue. | `--quantum` |
| `wfq` | WFQ-like: priority maps to a weight, requests are ordered by virtual finish tag (approximation). | `priority` column |

`wfq` computes `weight = WeightForPriority(priority)` and
`finish_tag = max(previous_tag, virtual_time) + service / weight`; the smallest finish tag is served
next. This is a simplified approximation of weighted fair queueing, not the full WFQ packet algebra.

---

## 4. Metrics

Per request (`output/result_<scheduler>.csv`):

| Column | Definition |
| --- | --- |
| `arrival_time` | Submission time of the request. |
| `start_time` | First instant the request receives service. |
| `finish_time` | Instant the last byte of the request is served. |
| `response_time` | `start_time - arrival_time`. |
| `turnaround_time` | `finish_time - arrival_time`. |
| `waiting_time` | `max(0, turnaround_time - total_processing_time)`. |
| `assigned_thread` | Worker that served the request. |

Benchmark summary (`output/summary.csv`):

| Column | Definition |
| --- | --- |
| `avg_waiting_time`, `avg_turnaround_time`, `avg_response_time` | Averages over completed requests. |
| `throughput` | `requests / total_completion_time` (requests per simulated second). |
| `fairness_index` | Jain's index over per-client throughput (`file bytes served / client service span`); `1.0` = perfectly even, lower = more skewed. |
| `total_completion_time` | Time from simulation start to the last completion (makespan). |

All durations are **simulated** seconds. Wall-clock time is never reported as a result: the worker
threads carry the event loop and use bounded sleeps only to emulate dispatch, so runtimes and metrics
are independent of machine speed.

---

## 5. Technology stack

| Layer | Choice |
| --- | --- |
| Core simulator | C++17, C++ standard library, `std::thread` / `std::mutex` / `std::condition_variable` |
| Build | GNU Make and CMake (both maintained), CTest for the test target |
| Tests | Assertion-based unit tests (`tests/test_simulator.cpp`) covering each scheduler and the metric collector |
| Data I/O | Hand-written CSV reader/writer (`include/csv_utils.h`) — no third-party CSV dependency |
| Analysis | Python 3 + matplotlib (`scripts/plot_summary.py`) |
| Browser demo | Hand-written vanilla JavaScript + Canvas (`docs/engine.js`, `docs/app.js`), no framework, no build step |
| Delivery | GitHub Pages (static site in `docs/`), PWA manifest + service worker, GitHub Actions CI |

The simulator itself has no third-party dependencies; Python is only needed for the plotting script.

---

## 6. Build

Requires a C++17 compiler and either Make or CMake ≥ 3.16.

```bash
# Make
make            # builds ./simulator
make test       # builds and runs ./simulator_tests

# CMake
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 7. Command-line reference

```text
./simulator [options]

--input <path>             Input request CSV
--scheduler <name>         fifo | sjf | priority | rr | wfq
--threads <int>            Number of worker threads
--quantum <float>          Round Robin time quantum in simulated seconds
--overhead <float>         Per-dispatch connection overhead in simulated seconds
--benchmark                Run all schedulers on the same input
--output-dir <path>        Output directory (default: output)
--generate-sample <path>   Generate a request CSV and exit
--n <int>                  Number of generated requests
--seed <int>               Random seed for generation
--help                     Show usage
```

Examples:

```bash
# Single scheduler
./simulator --input data/sample_requests.csv --scheduler fifo --threads 4

# Round Robin with a 1.0 s quantum and 0.05 s dispatch overhead
./simulator --input data/sample_requests.csv --scheduler rr --threads 4 --quantum 1.0 --overhead 0.05

# All five schedulers on one workload, compared in output/summary.csv
./simulator --input data/sample_requests.csv --benchmark --threads 4

# Deterministic workload generation
./simulator --generate-sample data/generated_requests.csv --n 100 --seed 42
```

---

## 8. Datasets

| File | Requests | Description |
| --- | --- | --- |
| `data/sample_requests.csv` | 15 | Compact hand-written workload used by the README examples and the default demo view. |
| `data/generated/n15_seed99.csv` | 15 | Generated with `--n 15 --seed 99`. |
| `data/generated/n37_seed2026.csv` | 37 | Generated with `--n 37 --seed 2026`. |
| `data/generated/n50_seed7.csv` | 50 | Generated with `--n 50 --seed 7`. |
| `data/generated/n120_seed42.csv` | 120 | Generated with `--n 120 --seed 42`. |
| `data/edge_cases.csv` | 18 | Boundary workload (identical arrival times, single-thread and high-thread cases, zero overhead, extreme priorities) used by the equivalence scenarios. |

Generation is seeded and therefore deterministic: the same `--n`/`--seed` pair always yields the same
CSV. The demo loads byte-identical copies of these files from `docs/data/`; `tools/sync_docs_data.sh`
refreshes them and prints SHA-256 digests so the two trees can be compared.
`data/reference/oracle_sha256.txt` holds the digests of the 72 CSV files the C++ engine produces for
the equivalence scenarios in §1.1.

---

## 9. Reproducing the results and checking the demo

| Tool | Purpose |
| --- | --- |
| `tools/run_oracle.sh` | Rebuilds the simulator, recomputes the 12 reference scenarios into `oracle/`, and refreshes (or with `--check`, verifies) the digest manifest `data/reference/oracle_sha256.txt`. |
| `tools/build_oracle_bundle.py` | Packs the reference CSVs into `docs/oracle_bundle.js` for `verify.html`. |
| `tools/sync_docs_data.sh` | Copies the canonical workloads into `docs/data/`; `--check` verifies the two trees are byte-identical (used in CI). |
| `tools/check_demo.py` | Headless-Chrome check of a local or deployed demo: runs `index.html` to completion and executes the full `verify.html` comparison. Prints a pass/fail summary. |
| `tools/ui_smoke.html` | Browser harness that drives the demo UI (workload switching, playback controls, table rendering, canvas painting) and reports the individual steps. |
| `tools/make_icons.py` | Regenerates the PWA icons. |

```bash
# Recompute the reference scenarios and refresh the demo data/bundle
./tools/run_oracle.sh
./tools/sync_docs_data.sh
python3 tools/build_oracle_bundle.py

# Serve the repository and check the demo (index.html + verify.html) in headless Chrome
python3 -m http.server 8000 &
python3 tools/check_demo.py http://127.0.0.1:8000/docs/

# Drive the UI harness
#   open http://127.0.0.1:8000/tools/ui_smoke.html in a browser

# Check the deployed site
python3 tools/check_demo.py https://tewei02.github.io/multithreaded-file-transfer-scheduler-simulator/
```

`tools/check_demo.py` needs Google Chrome and `pip install websocket-client`. The browser harnesses
are development aids; the reference bundle in `docs/oracle_bundle.js` plus `verify.html` are the
committed equivalence record, and `verify.html` re-runs in any modern browser without extra tooling.

---

## 10. Tests and CI

- **Unit tests** (`tests/test_simulator.cpp`): per-scheduler ordering behaviour, Round Robin slicing,
  WFQ tag ordering, summary metric computation, CSV round-trip. Run with `make test` or
  `ctest --test-dir build --output-on-failure`.
- **CI** (`.github/workflows/ci.yml`): CMake and Make builds, `ctest`, a benchmark smoke run that
  validates the `output/summary.csv` header and one row per scheduler plus the per-request row counts,
  a plotting smoke run, a `tools/run_oracle.sh --check` pass that reproduces the 72 reference digests,
  and a `tools/sync_docs_data.sh --check` pass with `git diff --exit-code -- docs/` so the committed
  demo assets and reference bundle cannot drift from the C++ engine.
- **Equivalence check**: `verify.html` / `tools/check_demo.py`, as described in §1.1.

---

## 11. Project structure

```text
multithreaded-file-transfer-scheduler-simulator/
├── .github/workflows/ci.yml        # build, tests, benchmark + plotting smoke
├── CMakeLists.txt                  # CMake build and CTest target
├── Makefile                        # make / make test
├── LICENSE                         # MIT
├── README.md
├── include/
│   ├── request.h                   # request model, thread-state model, metrics structs
│   ├── scheduler.h                 # IScheduler interface + five policies
│   ├── worker_manager.h            # worker pool and thread assignment
│   ├── simulation_engine.h         # discrete-event loop
│   ├── metrics.h                   # metric collector
│   └── csv_utils.h                 # CSV reader / writer
├── src/
│   ├── main.cpp                    # CLI entry point
│   ├── request.cpp
│   ├── scheduler.cpp
│   ├── worker_manager.cpp
│   ├── simulation_engine.cpp
│   ├── metrics.cpp
│   └── csv_utils.cpp
├── tests/test_simulator.cpp        # unit tests
├── data/                           # workloads (bundled, deterministic)
│   ├── sample_requests.csv
│   ├── edge_cases.csv
│   ├── generated/                  # seeded datasets
│   └── reference/oracle_sha256.txt # digests of the C++ reference outputs
├── docs/                           # GitHub Pages site (interactive demo + PWA)
│   ├── index.html
│   ├── app.js, engine.js, styles.css
│   ├── verify.html, oracle_bundle.js
│   ├── manifest.json, sw.js, icons/
│   └── data/                       # copies of the bundled workloads
├── scripts/plot_summary.py         # benchmark figures
├── tools/                          # reproducibility, verification and maintenance scripts
└── output/                         # generated CSVs and figures (git-ignored)
```

---

## 12. Design assumptions and limitations

- **Simulation only.** No sockets are opened and no bytes are transferred; file sizes and bandwidths
  are model parameters.
- **Event-driven clock.** Simulated time advances only at arrivals, dispatches and completions.
  Worker threads execute the event loop with bounded sleeps to emulate dispatch; wall-clock timing is
  not part of any result.
- **Service model.** Non-Round-Robin policies serve a request in a single non-preemptive slice, while
  Round Robin slices service by the quantum and re-queues unfinished work. Every dispatch adds
  `--overhead` simulated seconds of connection setup.
- **WFQ approximation.** The virtual-finish-tag implementation captures weighted ordering but not the
  full WFQ scheduling algebra.
- **Scope of the evaluation.** The metrics are outputs of the queuing model on the bundled workloads.
  The project reports no measurements from a production server, no user study and no field data, and
  none should be inferred from the figures.

---

## 13. Roadmap

- Additional policies (EDF, DRR, MLFQ) behind the existing `IScheduler` interface.
- Export of the Gantt timeline as CSV in addition to the on-screen canvas.
- Richer workload models (bursty arrivals, heterogeneous bandwidth, multi-server stages) and stress
  scenarios.
- Performance-regression checks in CI across the reference scenarios.

---

## License

Released under the [MIT License](LICENSE). © 2026 Te-Wei Ko.
