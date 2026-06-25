# Antigravity Kernel & Operating System Simulator

[![C++20 Standard](https://img.shields.io/badge/Language-C%2B%2B20-00599C.svg?style=for-the-badge&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![FTXUI Powered](https://img.shields.io/badge/Interface-FTXUI%20TUI-00d1b2.svg?style=for-the-badge)](https://github.com/ArthurSonzogni/FTXUI)
[![Build Verification](https://img.shields.io/badge/Verification-Formal%20Suite%20Passing-2ea44f.svg?style=for-the-badge)](#visual-verification)
[![MIT License](https://img.shields.io/badge/License-MIT-blueviolet.svg?style=for-the-badge)](LICENSE)

A high-concurrency, deterministic C++20 kernel simulation engine modeling multi-core preemptive CPU scheduling, hierarchical virtual memory paging, deadlock avoidance protocols, and autonomous load shedding under working-set thrashing.

---

## 1. Executive Summary

Modern operating system design requires negotiating fundamental tensions between strict liveness guarantees and maximum hardware resource saturation. The **Antigravity OS Simulator** models these foundational kernel trade-offs within a unified, high-performance execution architecture.

Designed for systems engineering research and advanced academic modeling, the simulator pairs a multi-core CPU scheduling dispatcher with a virtual memory subsystem capable of simulating physical memory constraints, hardware page faults, and pluggable eviction strategies. To eliminate catastrophic concurrent failure modes, the kernel enforces Dijkstra's Banker's Algorithm for deadlock avoidance, recursive Transitive Priority Inheritance (PIP) to prevent unbounded priority inversion, and an autonomous feedback mechanism that dynamically sheds load before working-set thrashing collapses system throughput.

---

## 2. Visual Verification

The simulator architecture is formally verified through both qualitative real-time observation and quantitative headless assertion suites.

### System Dashboard (FTXUI Real-Time Telemetry)
*The interactive 3-panel terminal interface displaying live CPU scheduling queues, physical RAM page frame flash countdowns, and real-time Banker allocation matrices.*

![System Dashboard](docs/assets/os%20simulator%20dashboard.png)

### Formal Verification Suite Confirmation
*Headless verification harness execution proving complete memory safety, deadlock avoidance, and load-shedding stabilization.*

![Test Verification](docs/assets/os%20simulator%20thrashing%20test.png)

---

## 3. Core Architectural Pillars

### I. Deadlock Avoidance Engine (Banker's Algorithm)
Rather than employing optimistic concurrency control that risks system deadlocks, the `ConcurrencyManager` enforces strict runtime safety state verification. Prior to granting multi-dimensional resource requests ($R_0, R_1, R_2$), the kernel evaluates simulated allocation vectors against Dijkstra's Banker's Algorithm. If an allocation would transition the system into an unsafe state where completion liveness cannot be guaranteed, the request is blocked or deferred atomically.

### II. Transitive Priority Inheritance Protocol (PIP)
Preemptive priority schedulers are inherently vulnerable to priority inversion when high-priority tasks block on synchronization mutexes held by lower-priority threads. The kernel resolves this by constructing a real-time Wait-For dependency graph (`Resource Allocation Graph`). When lock contention occurs, priority boosts transitively propagate across multi-tiered wait chains ($P_{\text{High}} \rightarrow P_{\text{Med}} \rightarrow P_{\text{Low}}$), ensuring bounded handoff latencies and starvation freedom.

### III. Autonomous Thrashing Mitigation Engine
When physical frame allocations fail to contain active working sets, operating systems experience thrashing—spending disproportionate cycles servicing page faults rather than executing process bursts. The simulator implements closed-loop telemetry between the `MemoryManagementUnit` and `ThrashingDetector`. When sliding-window fault frequencies breach critical thresholds ($\ge 80\%$), the engine dynamically suspends the process exhibiting the highest remaining CPU burst demand, immediately liberating frame working sets and restoring system liveness.

---

## 4. Key Features & Technologies

* **Language & Standard:** Built strictly in **C++20**, leveraging concepts, standard shared mutexes (`std::shared_mutex`), RAII ownership semantics (`std::unique_ptr`), and atomic synchronization primitives.
* **Pluggable Paging Subsystem:** Implements polymorphic page replacement algorithms Behind an abstract strategy interface:
  * First-In-First-Out (**FIFO**)
  * Least Recently Used (**LRU**)
  * Clock / Second Chance (**CLOCK**)
  * Belady's Theoretical Optimal (**OPT**)
* **Terminal User Interface (TUI):** Engineered with [FTXUI](https://github.com/ArthurSonzogni/FTXUI) for high-framerate, thread-safe rendering of kernel data structures.
* **Build Configuration:** Fully structured for modern build pipelines using **CMake (3.20+)** and **Ninja / Make** build generators.

### Algorithmic Complexity & Theoretical Significance

| Kernel Subsystem | Mechanism / Algorithm | Asymptotic Complexity | Theoretical Systems Significance |
| :--- | :--- | :--- | :--- |
| **CPU Dispatcher** | Round Robin Time-Slicing | $O(1)$ | Guarantees bounded wait times and starvation freedom. |
| **CPU Dispatcher** | Shortest Remaining Time (SRTF) | $O(\log n)$ | Provably minimizes system-wide average turnaround time. |
| **Mutex Monitor** | Transitive PIP Traversal | $O(d)$ | Limits priority inversion latency across dependency depth $d$. |
| **Deadlock Monitor** | Banker's Algorithm Verification | $O(m \cdot n^2)$ | Guarantees mathematically provable deadlock-free liveness. |
| **Paging Hierarchy** | LRU & Clock Eviction | $O(1)$ | Approximates optimal working-set cache hit ratios O(1). |
| **Resilience Engine** | Thrashing Load Shedding | $O(n)$ | Prevents throughput collapse under severe RAM oversubscription. |

---

## 5. Challenges Log

Engineering a highly concurrent operating system simulator introduces complex memory safety and synchronisation gotchas. Demonstrated system debugging victories documented within [CHALLENGES.md](CHALLENGES.md) include:

* **Challenge 5: Autonomous Thrashing Mitigation & Load Shedding:** Diagnosed and resolved non-deterministic `Segmentation fault` crashes caused by race conditions between asynchronous MMU fault rate telemetry callbacks and synchronous CPU scheduler execution ticks. Eliminated out-of-bounds heap corruption across uninitialized Banker allocation vectors through defensive schema bounds normalization.
* **Global Extern Linkage Resolution:** Managed cross-translation-unit state pointers (`extern g_cm`) across isolated verification test harnesses.
* **Recursive Priority Boosting:** Solved priority accumulation anomalies during nested mutex handoffs.

For comprehensive architectural root-cause analyses and GDB backtrace dissections, read [CHALLENGES.md](CHALLENGES.md).

---

## 6. Build Instructions

### Prerequisites
* **Compiler:** Clang 13+, GCC 11+, or MSVC 2022+ (Must support full C++20 features).
* **Build Tool:** CMake 3.20+.

### Compilation (CMake / Ninja)

```bash
# 1. Clone the repository
git clone https://github.com/renny6/os-simulator.git
cd os-simulator

# 2. Configure build tree (Release optimizations recommended)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build kernel simulation executable and test suites
cmake --build build --config Release
```

### Execution & Verification

```bash
# Launch interactive real-time FTXUI TUI Dashboard
./build/os_simulator

# Execute headless Formal Verification & Stress Testing suites
./build/thrashing_stress_test
./build/priority_test
./build/stress_test
```
