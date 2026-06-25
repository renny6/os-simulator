# **DOCUMENT 06 — IMPLEMENTATION PLAN**

# **Exact step-by-step build sequence with done criteria**

---

## **Phase 1 — Project Setup**

Goal: Clean build system that compiles an empty main() and fetches dependencies.

Tasks: 1.1 Create os\_simulator/ directory and all subdirectories 1.2 Write CMakeLists.txt with FetchContent for FTXUI v5.0.0 and yaml-cpp 0.8.0 1.3 Write config.yaml with all 5 processes and full parameter set 1.4 Write main.cpp that includes yaml-cpp, parses config.yaml, prints "Config loaded: N processes" and exits cleanly 1.5 Build and verify: mkdir build && cd build && cmake \-G Ninja .. && ninja

Done criteria:

* cmake .. completes without errors  
* ninja builds without errors or warnings  
* ./os\_simulator prints "Config loaded: 5 processes" and exits with code 0

---

## **Phase 2 — Core Data Structures (Headers Only)**

Goal: All headers written and internally consistent before any .cpp files.

Tasks: 2.1 include/utils/config\_parser.h — ConfigData struct \+ ConfigParser class declaration 2.2 include/utils/telemetry.h — CycleMetrics struct \+ TelemetryLogger class declaration 2.3 include/core/process.h — ProcessState enum, PCBSnapshot struct, PCB class declaration 2.4 include/memory/page\_replacer.h — PageFrame struct, PageReplacer abstract base class 2.5 include/memory/fifo\_strategy.h 2.6 include/memory/lru\_strategy.h 2.7 include/memory/clock\_strategy.h 2.8 include/memory/opt\_strategy.h 2.9 include/memory/mmu.h — MemoryManagementUnit class declaration 2.10 include/core/cpu\_scheduler.h — CpuScheduler class declaration 2.11 include/core/concurrency\_manager.h — KernelMutex, BankersMatrix, ConcurrencyManager declarations 2.12 include/ui/dashboard.h 2.13 include/ui/memory\_view.h 2.14 include/ui/lock\_monitor.h 2.15 include/ui/telemetry\_view.h

Done criteria:

* All headers compile cleanly: g++ \-std=c++20 \-c include/core/process.h (no errors)  
* No circular includes  
* Every class has its complete public interface declared (no TODOs in headers)

---

## **Phase 3 — Utility Implementations**

Goal: Config parsing and telemetry work end-to-end.

Tasks: 3.1 src/utils/config\_parser.cpp — full yaml-cpp parsing, populates ConfigData 3.2 src/utils/telemetry.cpp — ring buffer, ofstream writer, SIGINT flush

Done criteria:

* ConfigParser correctly reads all 5 processes from config.yaml  
* ConfigParser exits with clear error on malformed YAML  
* TelemetryLogger writes a valid 3-column CSV header on construction  
* On push() with a full ring buffer, oldest entry is discarded (not a crash)

---

## **Phase 4 — Virtual Memory Manager**

Goal: All 4 page replacement algorithms correct and validated.

Tasks: 4.1 src/core/process.cpp — PCB constructor, get\_snapshot(), update methods 4.2 src/memory/fifo\_strategy.cpp 4.3 src/memory/lru\_strategy.cpp 4.4 src/memory/clock\_strategy.cpp 4.5 src/memory/opt\_strategy.cpp 4.6 src/memory/mmu.cpp — translate(), handle\_fault(), TLB logic, thrashing detection

Done criteria:

* Run the validation trace \[1,2,3,4,2,3,5,6,2,1,2,3,7,6,3,3,1,2,3,6\] with 3 frames.  
* FIFO must produce exactly 15 faults.  
* LRU must produce exactly 12 faults.  
* Clock must produce exactly 12 or 13 faults (implementation-dependent, document which).  
* OPT must produce exactly 8 faults (known optimal for this trace with 3 frames).  
* Clock pointer never goes out of bounds after frame count change.  
* LRU timestamps are strictly monotonic (no ties possible).

---

## **Phase 5 — CPU Scheduler**

Goal: RR and SRTF dispatch processes correctly over simulated cycles.

Tasks: 5.1 src/core/cpu\_scheduler.cpp — tick(), context\_switch(), both algorithm modes

Done criteria:

* Round Robin with quantum=3 dispatches processes in correct rotation.  
* Process that finishes exactly on quantum expiry is TERMINATED, not re-queued.  
* SRTF always dispatches the process with the lowest remaining\_burst\_time.  
* Starvation counter increments for every READY cycle without dispatch.  
* Switching algorithms mid-simulation (S key) takes effect next cycle.

---

## **Phase 6 — Concurrency Manager**

Goal: Banker's Algorithm, Priority Inheritance, and lock monitor state all correct.

Tasks: 6.1 src/core/concurrency\_manager.cpp — KernelMutex, BankersMatrix, priority boost/revert

Done criteria:

* At least one resource request in config.yaml produces an UNSAFE STATE denial.  
* Denied request is logged as deadlock\_prevented in CycleMetrics.  
* Priority boost is applied atomically before scheduler re-evaluates.  
* Priority revert happens via PriorityGuard destructor — never missed.  
* Semaphore token count never exceeds construction-time maximum.  
* Lock ordering (ready\_queue → KernelMutex → PCB → telemetry) enforced in all methods.

---

## **Phase 7 — TUI Frontend**

Goal: Live dashboard renders all backend state correctly, all keypresses work.

Tasks: 7.1 src/ui/memory\_view.cpp — physical memory grid with flash animation 7.2 src/ui/lock\_monitor.cpp — traffic lights, wait queues, \[^\] indicator 7.3 src/ui/telemetry\_view.cpp — PCB table, hit rate gauge, CPU utilisation gauge 7.4 src/ui/dashboard.cpp — main layout, keypress handler, screen.Loop() entry 7.5 main.cpp (final) — wire everything together, launch kernel thread, register SIGINT handler, call screen.Loop()

Done criteria:

* SPACE advances exactly one cycle and UI updates without flicker.  
* R resets all state to Cycle 0 values correctly.  
* Q writes CSV, prints confirmation, exits cleanly.  
* TAB cycles focus between frame slider and quantum input.  
* Frame slider change takes effect within one cycle without any crash.  
* Belady's Anomaly label appears correctly in the FIFO test scenario.  
* Priority boost \[^\] indicator appears and disappears at the correct moments.  
* UI thread never calls PCB methods directly — only via get\_snapshot().  
* Terminal below 120×36 shows clear error and exits cleanly.

---

## **Phase 8 — Integration Testing**

Goal: End-to-end simulation runs correctly for 500+ cycles without any crash.

Tasks: 8.1 Run full simulation with all 5 config.yaml processes, RR scheduling, FIFO. 8.2 Switch to SRTF mid-run. Verify queue re-sorts correctly. 8.3 Switch page algorithm to LRU, CLOCK, OPT in sequence. Verify fault counts change. 8.4 Drag frame slider down to 1 frame. Verify thrashing detection fires after warmup. 8.5 Run Ctrl+C mid-simulation. Verify CSV is complete and parseable. 8.6 Run the validation trace in isolation. Verify all 4 algorithm fault counts match. 8.7 Simulate abnormal process termination (out-of-bounds page request). Verify host process does not crash and remaining processes continue.

Done criteria:

* Zero crashes over 500 simulated cycles on 5 processes.  
* CSV opens in LibreOffice Calc / Excel without errors.  
* All validation trace fault counts match documented expected values.  
* Ctrl+C produces a complete, valid CSV.

---

## **Phase 9 — Polish and Final Verification**

Goal: Project is ready to be shown to a hiring manager or submitted as coursework.

Tasks: 9.1 Write README.md with: project description, build instructions, keybindings, config.yaml field reference, known behaviours (Belady's, starvation). 9.2 Verify fresh build from scratch on clean Ubuntu 22.04 system. 9.3 Run with narrow terminal (80 cols). Verify clean error, not a crash. 9.4 Comment every non-obvious algorithm section with a reference e.g. // Banker's safety algorithm — Dijkstra 1965, see Tanenbaum Ch. 6

Done criteria:

* Fresh clone \+ cmake \+ ninja produces working binary with zero manual steps.  
* README explains every keybinding and every config.yaml field.  
* All algorithm implementations have a reference comment linking to the theory.

