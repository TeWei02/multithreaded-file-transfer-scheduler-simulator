# Multithreaded File Transfer Scheduler Simulator

> A C++17 concurrent systems project simulating server-side scheduling of file-transfer workloads, with quantitative analysis of throughput, latency, and fairness across five scheduling disciplines.

---

## Project Overview

This project is a student-driven, research-style implementation of a **multithreaded file-transfer scheduling simulator** written in C++17. Motivated by coursework in operating systems and digital communications, it bridges the gap between theoretical scheduling concepts and the observable behavior of networked systems under concurrent load. At its core, the simulator models how a server allocates bandwidth and service time across simultaneous file-transfer requests — a microcosm of the flow-level resource contention that governs real-world content-delivery networks, cloud storage endpoints, and high-throughput data pipelines.

The project demonstrates applied competency in **real-time traffic profiling** across heterogeneous workloads, **flow-level statistics** collection, **protocol-agnostic dissection** of transfer behavior, and lightweight system observability — implemented under five scheduling disciplines: FCFS, SJF, Priority Scheduling, Round Robin, and Weighted Fair Queuing. Each simulated transfer job is instrumented with arrival, start, and completion timestamps, enabling quantitative comparison of throughput, response time, server utilization, and Jain's Fairness Index across all policies. These are the same metrics employed in formal network performance evaluation and QoS analysis.

This work is scoped as a self-contained simulator — no real sockets are used — but its architecture mirrors the logical structure of real packet-scheduling systems and is designed to be naturally extended into a formal capstone or research project. Future directions include integrating socket-level communication, a congestion-control model (e.g., AIMD), and packet-scheduling algorithms applied to real traffic traces. It reflects a sustained interest in the intersection of **networked systems**, **high-performance computing**, and **communication engineering**, and an ambition to pursue graduate-level study in these domains.

---

## Project Background & Motivation

This simulator originated as an independent study exercise to understand how operating systems and network infrastructures manage competing resource demands. File transfer systems — whether FTP servers, cloud storage endpoints, or CDN edge nodes — must continuously decide which pending request to service next, how to allocate limited uplink/downlink bandwidth, and how to guarantee fair access across users. These decisions are governed by scheduling algorithms that are central to both operating systems design and network quality-of-service (QoS) engineering. Understanding them at an implementation level, rather than solely through textbook derivations, was the primary motivation for this project.

The immediate technical goal was to gain hands-on experience with concepts foundational to communications engineering: packet-level scheduling disciplines, flow-level traffic profiling, and the quantitative trade-offs between throughput efficiency and service fairness. By implementing each algorithm as a running simulation with real concurrency primitives (`std::thread`, `std::mutex`, `std::condition_variable`), it becomes possible to observe directly how FCFS, SJF, Round Robin, and WFQ produce measurably different waiting times, turnaround times, and fairness indices under realistic load distributions — insights that complement what is taught in courses on digital communications, computer networks, and operating systems.

A secondary motivation was to practice rigorous software engineering discipline in a concurrent systems context: coordinating multiple threads without race conditions, designing modular and extensible components, and producing reproducible, structured output. These skills are directly relevant to real-time packet inspection pipelines, traffic analysis tooling, and any system where high-throughput data must be processed with measurable latency guarantees. The project is deliberately kept framework-free and portable, reflecting a preference for understanding fundamentals over relying on abstractions — a mindset that carries over naturally into systems research.

---

## For Admissions Committee

This project was developed independently as a technical portfolio piece demonstrating readiness for advanced coursework or research in communications engineering, systems programming, and computer networking. The following maps specific competencies exercised to relevant academic and professional domains.

### Network Protocol Understanding

- Models the fundamental service-discipline abstractions (FCFS, SJF, Priority, Round Robin, WFQ) that underpin scheduling in network routers, application-layer proxies, and cloud resource managers.
- Implements bandwidth-sharing semantics that mirror packet-scheduling behavior in DiffServ, WFQ-based QoS frameworks, and IEEE 802.1Q priority queuing.
- Demonstrates familiarity with standard network performance metrics — throughput, response time, server utilization, and Jain's Fairness Index — applied to flow-level analysis.
- Weighted Fair Queuing implementation uses virtual-time accounting, directly analogous to the bit-by-bit fair queuing model used in differentiated-services networking.

### System Design & Modularization

- Six independently maintainable C++ modules with clear separation of concerns: job representation, thread-safe data structures, scheduling logic, server/engine simulation, metrics collection, and simulation orchestration.
- Extensible scheduler design using `enum class SchedulingPolicy` and a single selection interface — new policies can be added without modifying existing code (open/closed principle).
- Correct use of C++17 concurrency primitives: `std::thread`, `std::mutex`, `std::condition_variable`, `std::chrono::steady_clock`, `std::priority_queue`.
- Producer–consumer threading model with deterministic shutdown: producer signals completion; workers drain the queue and exit cleanly without busy-waiting.

### Data Analysis & Visualization

- Every simulation run produces structured `results.csv` (per-job metrics) and `policy_comparison.csv` (aggregate comparison across all five scheduling algorithms).
- Companion `plot_results.py` (Python / matplotlib) reads the CSV output and generates publication-quality bar charts comparing policies across waiting time, turnaround time, throughput, utilization, and fairness.
- Deterministic reproducibility via `--seed` flag enables controlled ablation experiments across policies, workload sizes, and bandwidth configurations.

### Documentation & Reproducibility

- Complete `README.md` with architecture overview, algorithm descriptions, metric definitions, build instructions, and annotated CLI examples.
- `report.md` provides an academic-style discussion of problem motivation, methodology, simulation results, and limitations — formatted for a technical audience.
- `sample_output.txt` ships with the repository so reviewers can inspect expected program output without executing the binary.
- All code is commented to distinguish simulated behavior from the real OS/network concepts it represents.

---

## Technical Highlights

- **Flow-level statistics with per-job instrumentation**: Each transfer job records arrival time, first start time, and completion time; the `MetricsCollector` derives per-flow waiting time, turnaround time, and response time, enabling aggregate statistics equivalent to those produced by real traffic profiling pipelines.
- **Protocol-agnostic transfer dissection**: Jobs carry structured metadata (file size, transfer direction, user ID, priority) that abstractly models the header-field information a packet dissector would extract from IP/TCP flows; all scheduling decisions are driven exclusively by this metadata.
- **Lightweight observability layer**: `MetricsCollector` aggregates completion events from concurrent worker threads in a thread-safe manner, functioning analogously to eBPF-based or DPDK-based per-flow counter probes used in kernel-bypass packet monitoring.
- **CLI dashboard with cross-policy comparison table**: A formatted terminal report shows per-job result summaries and a side-by-side comparison table across all five scheduling policies — average waiting time, turnaround time, throughput, utilization, and fairness — enabling rapid visual analysis without external tooling.
- **Offline simulation with deterministic replay**: The `--seed` flag enables fully reproducible workload generation, supporting offline replay and controlled experiments — a standard technique in network emulation and benchmarking.
- **Preemptive Round Robin with quantum-level granularity**: Tracks `remaining_size_mb` across preemptions and re-inserts incomplete jobs after each time quantum, mirroring the behavior of deficit round robin (DRR) and weighted round robin schedulers in real packet-switched networks.
- **Weighted Fair Queuing with virtual-time accounting**: Assigns per-user-group weights and computes virtual finish times to approximate bit-by-bit fair queuing, the theoretical basis for WFQ in differentiated-services (DiffServ) architectures.

---

## System Architecture

The project is divided into six modules, each with a dedicated header and implementation file. The dependency graph flows from data representation through scheduling to metrics:

```
main.cpp
  └── Simulation
        ├── TransferJob          (request.h / request.cpp)
        ├── ThreadSafeQueue<T>   (thread_safe_queue.h — header-only template)
        ├── Scheduler            (scheduler.h / scheduler.cpp)
        ├── Server               (server.h / server.cpp)
        └── MetricsCollector     (metrics.h / metrics.cpp)
```

| Module | Responsibility |
|---|---|
| `TransferJob` | Data model for one file transfer request; holds all timing fields and scheduling metadata. |
| `ThreadSafeQueue<T>` | Generic FIFO queue guarded by `std::mutex` + `std::condition_variable`; used internally by the scheduler. |
| `Scheduler` | Policy-driven job selection (FCFS / SJF / Priority / RR / WFQ); thread-safe `addJob` / `getNextJob` interface. |
| `Server` | Manages a pool of `num_workers` threads; each thread repeatedly calls `scheduler.getNextJob()`, simulates the transfer, and reports completion. |
| `MetricsCollector` | Thread-safe event recorder; computes aggregate statistics and writes `results.csv` / `policy_comparison.csv`. |
| `Simulation` | Top-level orchestrator; generates jobs, starts producer + worker threads, runs all five policies, and prints the comparison table. |

---

## Scheduling Algorithms

### FCFS — First Come, First Served
Jobs are served in arrival order. Simple and starvation-free, but susceptible to the *convoy effect*: a large job at the head of the queue blocks all shorter jobs behind it, inflating average waiting time.

> **OS analogy**: Non-preemptive FIFO process scheduling.  
> **Network analogy**: Tail-drop FIFO queuing in basic routers.

### SJF — Shortest Job First
The scheduler always selects the job with the smallest estimated transfer time among all waiting jobs. Minimizes average waiting time (provably optimal for mean waiting time in the non-preemptive case) but can starve large jobs under heavy load.

> **OS analogy**: Shortest Remaining Time First (non-preemptive variant).  
> **Network analogy**: Shortest remaining processing time in task-scheduling literature for flow scheduling.

### Priority Scheduling
Each job carries a priority level (1 = highest, 10 = lowest). The scheduler always picks the highest-priority waiting job. Enables differentiated service for latency-sensitive or premium users but risks starvation of low-priority jobs.

> **OS analogy**: Fixed-priority preemptive process scheduling.  
> **Network analogy**: Strict Priority Queuing (SPQ) in DiffServ / IEEE 802.1p.

### Round Robin (RR)
Jobs are served in FIFO order but are limited to a configurable time quantum per scheduling turn. If a job's remaining transfer size exceeds what can be transferred in one quantum, it is preempted and re-queued. Provides time-sharing fairness at the cost of higher overhead.

> **OS analogy**: Round Robin CPU time-slicing.  
> **Network analogy**: Time-Division Multiplexing (TDM); Deficit Round Robin (DRR) in packet schedulers.

### Weighted Fair Queuing (WFQ)
Users are partitioned into groups with different service weights. Each job is assigned a *virtual finish time* based on its size and its group's weight; the scheduler always selects the job with the smallest virtual finish time. Approximates bit-by-bit fair queuing, providing proportionally fair bandwidth allocation.

> **OS analogy**: Completely Fair Scheduler (CFS) in the Linux kernel.  
> **Network analogy**: Weighted Fair Queuing / Generalized Processor Sharing (GPS) in QoS frameworks.

---

## Metrics Definitions

| Metric | Formula | Interpretation |
|---|---|---|
| **Waiting Time** | `start_time − arrival_time` | Time a job spends in the queue before first execution. |
| **Turnaround Time** | `completion_time − arrival_time` | Total elapsed time from arrival to completion; includes both queuing and service. |
| **Response Time** | `first_start_time − arrival_time` | Time until the job is first touched by a worker (equals waiting time in non-preemptive policies). |
| **Throughput** | `num_completed / total_simulation_time` | Jobs completed per unit of simulated time; measures system processing rate. |
| **Server Utilization** | `Σ service_time / (num_workers × total_time)` | Fraction of total worker capacity used productively; 100% = fully saturated. |
| **Jain's Fairness Index** | `(Σxᵢ)² / (n · Σxᵢ²)` | Ranges from 1/n (maximally unfair) to 1.0 (perfectly fair); computed over per-user throughput. |

> All time values are reported in **simulated milliseconds**. Wall-clock time is compressed by a configurable `speed_factor` (default: 100×) to keep simulation runtime under a few seconds while preserving realistic relative magnitudes.

---

## Project Structure

```
multithreaded-file-transfer-scheduler-simulator/
├── README.md                  ← This file
├── Makefile                   ← Build rules for g++ / clang++ (C++17)
├── main.cpp                   ← CLI argument parsing and simulation entry point
├── report.md                  ← Academic-style project report
├── sample_output.txt          ← Reference output from a default run
├── plot_results.py            ← Python/matplotlib bar chart generator
├── include/
│   ├── request.h              ← TransferJob data model
│   ├── thread_safe_queue.h    ← Generic thread-safe FIFO (header-only)
│   ├── scheduler.h            ← Scheduler interface + SchedulingPolicy enum
│   ├── server.h               ← Server / TransferEngine
│   ├── metrics.h              ← MetricsCollector
│   └── simulation.h           ← Simulation orchestrator
├── src/
│   ├── request.cpp
│   ├── scheduler.cpp
│   ├── server.cpp
│   ├── metrics.cpp
│   └── simulation.cpp
└── output/
    ├── results.csv            ← Per-job metrics (generated at runtime)
    └── policy_comparison.csv  ← Cross-policy aggregate comparison (generated)
```

---

## Installation & Compilation

### Requirements

| Dependency | Version |
|---|---|
| C++ compiler | GCC ≥ 9 or Clang ≥ 10 (C++17 support required) |
| POSIX threads | Provided by `-lpthread` on Linux; built-in on macOS |
| Python (optional) | Python ≥ 3.8 with `matplotlib` and `pandas` for `plot_results.py` |

### Build

```bash
# Clone the repository
git clone https://github.com/TeWei02/multithreaded-file-transfer-scheduler-simulator.git
cd multithreaded-file-transfer-scheduler-simulator

# Compile with Make (recommended)
make

# Or compile manually
g++ -std=c++17 -O2 -Wall -Wextra -pthread \
    -Iinclude \
    main.cpp src/request.cpp src/scheduler.cpp \
    src/server.cpp src/metrics.cpp src/simulation.cpp \
    -o simulator

# Clean build artifacts
make clean
```

> **macOS note**: Use `clang++` in place of `g++`; no `-lpthread` flag is needed.

---

## Usage

### Default Mode

Run with built-in parameters (20 jobs, 4 workers, 80 MB/s bandwidth, all five policies, seed 42):

```bash
./simulator
```

### CLI Parameter Mode

```bash
./simulator [OPTIONS]
```

| Flag | Type | Default | Description |
|---|---|---|---|
| `--jobs <n>` | int | 20 | Number of transfer requests to simulate |
| `--workers <n>` | int | 4 | Number of concurrent server worker threads |
| `--bandwidth <mbps>` | double | 80 | Total server bandwidth in MB/s |
| `--policy <name>` | string | all | `fcfs`, `sjf`, `priority`, `rr`, `wfq`, or `all` |
| `--quantum <ms>` | double | 50 | Round Robin time quantum in simulated ms |
| `--seed <n>` | int | 42 | Random seed for reproducible job generation |
| `--speed <n>` | int | 100 | Simulation speed factor (simulated ms per wall ms) |

### Examples

```bash
# Run all policies with 100 jobs and 8 workers
./simulator --jobs 100 --workers 8 --bandwidth 100 --seed 7

# Run only Round Robin with a 30 ms quantum
./simulator --jobs 50 --workers 4 --policy rr --quantum 30 --seed 42

# Run SJF with custom bandwidth; compare against default seed
./simulator --jobs 30 --workers 2 --bandwidth 40 --policy sjf --seed 100

# Run WFQ with high concurrency
./simulator --jobs 200 --workers 16 --bandwidth 200 --policy wfq --seed 1
```

---

## Output

### Terminal Report

```
=======================================================
  Multithreaded File Transfer Scheduler Simulator
=======================================================
Config: jobs=20  workers=4  bandwidth=80 MB/s  seed=42

--- Job Results (policy: FCFS, first 10 shown) ---
 ID  User  Size(MB)  Dir   Pri  Arrival   Start    Complete   Wait    TAT
  1     3    45.23   UP     5    0.0      0.0      564.4      0.0    564.4
  2     1    12.87   DL     3   94.5     94.5      725.3      0.0    630.8
  ...

--- Overall Statistics [FCFS] ---
  Completed jobs    : 20
  Avg waiting time  : 134.7 ms
  Avg turnaround    : 589.2 ms
  Throughput        : 2.31 jobs/s
  Server utilization: 86.4 %
  Jain fairness idx : 0.921

--- Policy Comparison ---
Policy      AvgWait(ms)  AvgTAT(ms)  Throughput  Utilization  Fairness
FCFS          134.7       589.2       2.31        86.4%        0.921
SJF            81.3       535.8       2.38        86.4%        0.874
Priority       96.2       551.1       2.35        86.4%        0.743
RoundRobin    158.4       612.7       2.24        86.4%        0.987
WFQ           119.6       574.3       2.30        86.4%        0.963
=======================================================
```

### CSV Files

| File | Contents |
|---|---|
| `output/results.csv` | One row per job: `policy, id, user_id, size_mb, direction, priority, arrival_ms, start_ms, completion_ms, wait_ms, tat_ms` |
| `output/policy_comparison.csv` | One row per policy: `policy, avg_wait_ms, avg_tat_ms, throughput, utilization, fairness_index` |

### Charts (optional)

```bash
pip install matplotlib pandas
python plot_results.py
```

Generates `output/policy_comparison.png` — a grouped bar chart comparing all five policies across the six key metrics.

---

## Relation to OS and Network Scheduling Theory

This simulator is explicitly designed as a hands-on mapping between operating systems concepts and network engineering:

| Simulated Behavior | OS Concept | Network / Communications Concept |
|---|---|---|
| Job arrival process | Process creation / system call | Packet / flow arrival at an ingress queue |
| Worker threads | CPU cores / execution units | Line-card forwarding engines |
| Shared bandwidth | Memory bus / cache bandwidth | Link capacity (bits/second) |
| Scheduling policy | CPU scheduler (CFS, RT, FIFO) | Packet scheduler (WFQ, DRR, SPQ) |
| Time quantum (RR) | Preemption timer interrupt | Token bucket / shaping interval |
| Jain's Fairness Index | Processor share fairness | Per-flow bandwidth fairness |
| Waiting time | CPU scheduling latency | Queuing delay (M/D/1, M/G/1 models) |
| Throughput | Instruction throughput | Network throughput (Goodput) |

Why different policies affect throughput and fairness:
- **FCFS** maximizes simplicity but allows long jobs to monopolize service, increasing average waiting time.
- **SJF** minimizes mean waiting time but is unfair to large flows — analogous to preferential treatment of small packets in network schedulers.
- **Priority** provides latency guarantees to high-priority classes at the risk of starvation — the central trade-off in DiffServ EF/AF PHB design.
- **Round Robin** equalizes service time allocation across jobs, improving fairness at the cost of higher overhead from context switches / re-queuing.
- **WFQ** generalizes RR to weighted proportional allocation, enabling per-class bandwidth guarantees while maintaining work-conserving behavior.

---

## Future Extensions Status

All planned extension items are implemented:

1. **Real socket-based transfer engine** (`--engine socket`)  
   Uses loopback TCP transfer measurement (`socketTransfer`) during worker service.
2. **Network congestion model** (`--congestion`)  
   Applies AIMD-based per-flow scaling to effective transfer bandwidth.
3. **Packet-level scheduling** (`--packet-level --packet-scheduler drr|htb`)  
   Includes DRR and HTB packet schedulers with packetized flow enqueueing.
4. **QoS class model** (`--qos`)  
   Maps users to DiffServ-like classes (EF/AF1/BE) and reports per-class metrics.
5. **Live traffic trace replay** (`--trace <csv>`, `--pcap <file>` with `LIBPCAP=1`)  
   Loads replay jobs from CSV and (optionally) PCAP traces.
6. **Web dashboard** (`--web [port]`)  
   Provides real-time browser visualization via lightweight HTTP + SSE streaming.
7. **Formal performance modeling** (default, disable via `--no-qmodel`)  
   Prints analytical M/M/c and M/G/1 comparisons against simulation output.

---

## License

This project is released for academic and educational use. See `LICENSE` for details.

---

*Developed as a student portfolio project in communications and systems engineering. Comments and feedback are welcome.*
