# Multithreaded File Transfer and Scheduling Simulator

A C++17 academic project that simulates concurrent file transfer requests on a server with limited worker threads, then compares scheduling strategies using reproducible metrics and CSV outputs.

## 1. Project Introduction

This simulator models the following scenario:

- Multiple clients submit file transfer requests.
- A server has a fixed number of worker threads.
- Each worker handles one request slice at a time.
- The scheduler decides dispatch order and resource sharing policy.

The project emphasizes systems and networking fundamentals: scheduling behavior, queueing delay, throughput, fairness, and reproducible benchmarking.

## 2. Features

- C++17 modular implementation.
- Five scheduling strategies:
1. FIFO
2. SJF (shortest estimated transfer time)
3. Priority Scheduling
4. Round Robin (time quantum)
5. WFQ-like (virtual finish-tag approximation)
- Two execution modes:
1. Single run mode (`--scheduler ...`)
2. Benchmark mode (`--benchmark`) to run all schedulers on the same input.
- CSV input parser + CSV output writer.
- Random sample dataset generator.
- Python plotting script for benchmark figures.
- Basic unit tests.
- GitHub Actions CI pipeline.

## 3. Architecture

Core modules:

- `Request / Task Model`: `include/request.h`
- `Scheduler Interface + Strategies`: `include/scheduler.h`, `src/scheduler.cpp`
- `Worker Manager`: `include/worker_manager.h`, `src/worker_manager.cpp`
- `Simulation Engine`: `include/simulation_engine.h`, `src/simulation_engine.cpp`
- `Metrics Collector`: `include/metrics.h`, `src/metrics.cpp`
- `CSV Utilities`: `include/csv_utils.h`, `src/csv_utils.cpp`
- `CLI Entry`: `src/main.cpp`

## 4. Scheduling Algorithms

### FIFO

Serve by arrival order.

### SJF

Pick the request with smallest estimated remaining transfer time.

### Priority

Pick the highest priority first (smaller number = higher priority).

### Round Robin

Use fixed `--quantum` in simulated seconds; unfinished requests are re-queued.

### WFQ-like

Use priority-derived weight and virtual finish tags. Smaller finish tag gets served first.

## 5. Build

### Build with Make

```bash
make
```

### Build with CMake

```bash
cmake -S . -B build
cmake --build build
```

## 6. Run Examples

```bash
# Single run
./simulator --input data/sample_requests.csv --scheduler fifo --threads 4

# Single run with Round Robin
./simulator --input data/sample_requests.csv --scheduler rr --threads 4 --quantum 1.0

# Benchmark all schedulers
./simulator --input data/sample_requests.csv --benchmark --threads 4

# Generate sample requests
./simulator --generate-sample data/generated_requests.csv --n 100 --seed 42
```

## 7. Output and Metrics

Per-request output (`output/result_<scheduler>.csv`) includes:

- `request_id`
- `arrival_time`
- `start_time`
- `finish_time`
- `waiting_time`
- `turnaround_time`
- `response_time`
- `assigned_thread`
- `scheduler_name`

Benchmark summary (`output/summary.csv`) includes:

- `scheduler`
- `avg_waiting_time`
- `avg_turnaround_time`
- `avg_response_time`
- `throughput`
- `fairness_index` (Jain's fairness index)
- `total_completion_time`

## 8. Plotting

```bash
python scripts/plot_summary.py --input output/summary.csv --output output/figures
```

Charts:

- average waiting time
- average turnaround time
- throughput
- fairness index

## 9. Unit Tests

### Make

```bash
make test
```

### CMake/CTest

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 10. CI (GitHub Actions)

Workflow file: `.github/workflows/ci.yml`

CI steps:

- Build with CMake
- Run unit tests via CTest
- Run simulator smoke benchmark
- Validate `output/summary.csv` generation
- Run Python plotting script

## 11. Experiment Report Template

A ready-to-fill report template is provided at:

- `docs/experiment_report_template.md`

Use it for course reports, portfolio documentation, or reproducible experiment write-ups.

## 12. Project Structure

```text
multithreaded-file-transfer-scheduler-simulator/
├── .github/workflows/ci.yml
├── CMakeLists.txt
├── Makefile
├── README.md
├── include/
├── src/
├── tests/
├── data/
├── output/
├── scripts/
└── docs/
```

## 13. Design Assumptions

- This is a simulation (no real socket transfer).
- Time progression is event-driven.
- Worker threads use only tiny sleep to emulate dispatch behavior.
- WFQ is a simplified but practical approximation for coursework.

## 14. Future Work

- Add more policies (e.g., EDF, DRR, MLFQ).
- Add Gantt timeline CSV export.
- Add richer workload models and stress scenarios.
- Add integration tests and performance regression checks.
