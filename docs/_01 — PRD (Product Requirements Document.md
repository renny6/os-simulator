# **DOCUMENT 01 — PRD (Product Requirements Document)**

# **What we are building and why**

---

App Name : OS Memory & Concurrency Simulator Tagline : A terminal-based simulator that makes an operating system's memory management and process scheduling visible in real time.

## **Problem**

Computer science students and self-taught developers learn OS concepts (page replacement, scheduling, deadlock avoidance) from textbooks with static diagrams. There is no tool that lets them watch these algorithms run live, step through them cycle by cycle, and see exactly what changes and why. The gap between reading about LRU and actually seeing a page get evicted is enormous.

## **Target User**

A second or third year computer science student, or a self-taught developer, who understands OS theory but has never seen it execute in front of them. They are comfortable with the terminal. They want to visualise what they have read. They are also a hiring manager's target: a student who builds this project signals serious systems-level competence.

## **Core Features — Must Have**

1. Multi-threaded kernel simulation using real C++ std::thread instances  
2. CPU Scheduler with Round Robin (RR) and Shortest Remaining Time First (SRTF)  
3. Virtual Memory Manager with FIFO, LRU, Clock, and OPT page replacement  
4. Deadlock avoidance via Dijkstra's Banker's Algorithm  
5. Priority Inheritance Protocol to prevent Priority Inversion  
6. Translation Lookaside Buffer (TLB) simulation with hit/miss ratio tracking  
7. Live TUI dashboard (FTXUI) with memory grid, lock monitor, PCB table  
8. Step-by-step mode: spacebar advances one clock cycle at a time  
9. Runtime slider to change physical frame count during simulation  
10. config.yaml drives all parameters — zero hardcoded values  
11. Telemetry CSV export on exit with cycle-by-cycle metrics  
12. Built-in validation suite that verifies known page fault counts on reference trace

## **Nice to Have — V2 Only**

* A "replay" mode that loads a saved CSV and re-renders the simulation  
* Colour-coded heat map showing which pages are "hot"  
* Side-by-side algorithm comparison mode (run FIFO and LRU simultaneously)  
* Export a PNG screenshot of the TUI at any cycle

## **Explicitly Out of Scope for V1**

* No GUI. Terminal only. No Electron, no web frontend, no Qt.  
* No real process execution. Everything is simulated data, not real OS calls.  
* No network simulation of any kind.  
* No persistence between runs other than the CSV export.  
* No user authentication, accounts, or cloud features.  
* No Windows support. Linux only (Ubuntu 20.04+).

## **User Stories**

* As a student, I want to press spacebar and watch exactly one clock cycle execute, so I can trace the algorithm step by step without it racing past me.  
* As a student, I want to see a page frame flash red when a page fault occurs and green when it is a hit, so I understand immediately what happened and why.  
* As a student, I want to drag a slider and reduce the frame count mid-simulation and watch the fault rate change, so I understand the relationship between RAM and page faults physically.  
* As a student, I want to see Belady's Anomaly labelled and explained in the UI when it occurs, so I learn from it rather than think the code is broken.  
* As a student, I want to watch the Banker's Algorithm deny an unsafe resource request in the lock monitor, so I can see deadlock avoidance working in real time.  
* As a student, I want a CSV report after each run so I can graph metrics and include the data in a coursework submission.

## **Success Metrics**

* All 4 page replacement algorithms produce fault counts that match textbook reference values on the validation trace.  
* Banker's Algorithm correctly identifies and denies at least one unsafe state per simulation run using the provided config.yaml process definitions.  
* TUI renders without flickering or tearing on a standard 120-column terminal.  
* The simulation can run for 1000+ cycles without memory growth (telemetry ring buffer caps memory usage).  
* The project compiles from a fresh Ubuntu 22.04 install with a single: mkdir build && cd build && cmake .. && cmake \--build .

