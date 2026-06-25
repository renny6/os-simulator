\# Project Specification: OS Memory & Concurrency Simulator

\#\# Overview  
A high-performance, terminal-based Operating System simulator written in C++. This project models the core responsibilities of a modern OS kernel, focusing on concurrent process execution, critical section synchronization, and virtual memory management. It utilizes hardware-level C++ threading, robust Object-Oriented architecture, and a live-updating Text User Interface (TUI).

\---

\#\# 1\. Concurrency & CPU Scheduling Engine  
This subsystem handles the lifecycle of processes and ensures safe execution in a multithreaded environment.

\* \*\*Multi-Threaded Kernel Engine:\*\* Utilizes \`std::thread\` to simulate independent processes running concurrently.  
\* \*\*Hardware Synchronization Locks:\*\* Implements \`std::mutex\` and \`std::unique\_lock\` to protect shared resources (like the physical memory array and ready queues) from race conditions.  
\* \*\*Semaphore Arbitrator:\*\* Uses \`std::counting\_semaphore\` to manage access to limited simulated hardware resources.  
\* \*\*CPU Scheduler:\*\* A configurable dispatcher that allocates CPU time to process threads using:  
  \* Round Robin (RR) with adjustable time slices.  
  \* Shortest Remaining Time First (SRTF).  
\* \*\*Deadlock Avoidance:\*\* Implements Dijkstra’s Banker’s Algorithm to pre-calculate safe/unsafe states before granting resource locks.  
\* \*\*Priority Inheritance Protocol:\*\* Actively prevents Priority Inversion by temporarily boosting the priority of lower-level threads that hold locks required by higher-priority threads.

\#\# 2\. Virtual Memory Manager (VMM)  
This component translates virtual addresses and handles page replacement strategies when simulated physical RAM is full.

\* \*\*Paged Memory Allocator:\*\* Manages a contiguous array representing physical RAM divided into fixed-size page frames.  
\* \*\*Dynamic Translation Lookaside Buffer (TLB):\*\* A high-speed cache simulation tracking recent virtual-to-physical mappings to compute TLB hit/miss ratios.  
\* \*\*Polymorphic Page Replacement Subsystem:\*\* An interface capable of dynamically swapping algorithms at runtime:  
  \* \*\*FIFO (First-In, First-Out):\*\* Evicts the oldest loaded page.  
  \* \*\*LRU (Least Recently Used):\*\* Evicts the page that hasn't been accessed for the longest time.  
  \* \*\*Clock (Second-Chance):\*\* A highly efficient circular buffer approximation of LRU.  
  \* \*\*OPT (Belády's Optimal):\*\* A look-ahead algorithm used as a mathematical baseline to calculate real-world algorithm efficiency.  
\* \*\*Thrashing Mitigation:\*\* Monitors page-fault frequency and suspends low-priority processes if the system spends more time paging than executing.

\#\# 3\. Interactive Text User Interface (TUI)  
A multi-panel, live-updating terminal dashboard built using the \`FTXUI\` library.

\* \*\*Live Interactive Controls:\*\* Allows the user to step through the CPU clock cycle-by-cycle (using the spacebar) or dynamically adjust physical frame capacity via sliders during execution.  
\* \*\*Physical Memory Map:\*\* A visual grid displaying the allocation state of every page frame. Visually flashes green for Page Hits and red for Page Faults, showing exactly how pages are evicted.  
\* \*\*Concurrency Lock Monitor:\*\* A diagnostic panel visualizing mutexes and semaphores as traffic lights, alongside a live visual queue of which specific process IDs (PIDs) are blocked and waiting.  
\* \*\*Active PCB Monitor:\*\* A live table showing the status of all active Process Control Blocks (State, Burst Time, Priority).

\#\# 4\. Configuration & Telemetry  
A data-driven subsystem that decouples simulation parameters from the source code.

\* \*\*YAML Configuration Loader:\*\* Uses \`yaml-cpp\` to parse a startup file (\`config.yaml\`) defining system RAM, page sizes, core counts, and incoming process memory requests.  
\* \*\*Automated Verification Suite:\*\* A built-in testing harness designed to validate algorithmic accuracy against standard reference sequences (e.g., verifying that the trace \`1, 2, 3, 4, 2, 3, 5, 6, 2, 1, 2, 3, 7, 6, 3, 3, 1, 2, 3, 6\` produces the exact mathematical number of expected faults in FIFO).  
\* \*\*High-Fidelity Telemetry:\*\* Captures metrics at every clock cycle, including CPU utilization percentage, memory fragmentation ratio, and localized page-fault frequency.  
\* \*\*CSV Post-Mortem Exporter:\*\* Automatically generates a detailed \`.csv\` report upon exit, logging total execution time, deadlocks prevented, and an aggregate Hit/Fault percentage.

\---

\#\# Technical Stack  
\* \*\*Language:\*\* C++ (C++20 recommended for \`std::counting\_semaphore\`)  
\* \*\*Build System:\*\* CMake  
\* \*\*UI Library:\*\* FTXUI (Functional Terminal eXtension)  
\* \*\*Configuration:\*\* yaml-cpp  
