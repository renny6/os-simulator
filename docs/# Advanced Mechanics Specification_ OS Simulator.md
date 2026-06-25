\# Advanced Mechanics Specification: OS Simulator

\#\# Overview  
This document outlines the implementation strategy for the four advanced subsystems of the OS Simulator. These features introduce real-world kernel complexity, transforming the project from a basic academic exercise into an industry-grade systems portfolio piece.

\---

\#\#\# 1\. Configurable CPU Scheduling (\`/core/cpu\_scheduler.cpp\`)  
The scheduler acts as the heartbeat of the simulator, dictating which thread gets access to the CPU based on user-defined rules.

\* \*\*Implementation Strategy:\*\*  
    \* \*\*The Ready Queue:\*\* A \`std::priority\_queue\` (for SRTF) or a \`std::queue\` (for Round Robin) that holds pointers to \`ProcessControlBlock\` (PCB) objects.  
    \* \*\*Round Robin (RR) Logic:\*\* Implement a clock cycle counter. When a process's execution time hits the user-defined \`time\_slice\_quantum\`, trigger a context switch: pause the thread, move the PCB to the back of the queue, and pop the next PCB.  
    \* \*\*Shortest Remaining Time First (SRTF) Logic:\*\* Every time a new process arrives, or a clock cycle ticks, the scheduler evaluates the \`remaining\_burst\_time\` of all available processes. The CPU is preempted and given to the process with the smallest value.

\#\#\# 2\. Priority Inversion & Inheritance (\`/core/concurrency\_manager.cpp\`)  
This prevents a high-priority thread from being indefinitely blocked by a low-priority thread holding a crucial \`std::mutex\`.

\* \*\*Implementation Strategy:\*\*  
    \* \*\*Lock Ownership Tracking:\*\* Create a custom wrapper around \`std::mutex\` called \`KernelMutex\`. When a thread locks it, the \`KernelMutex\` must record the PID and base priority of the owning thread.  
    \* \*\*The Inheritance Trigger:\*\* If Thread A (Priority High) attempts to lock a \`KernelMutex\` currently held by Thread B (Priority Low), the Concurrency Manager intervenes.   
    \* \*\*Temporary Boost:\*\* The Manager temporarily elevates Thread B's priority to match Thread A. Once Thread B unlocks the mutex, the Manager immediately reverts Thread B back to its base priority.

\#\#\# 3\. Belády's Optimal Algorithm (OPT) (\`/memory/opt\_strategy.cpp\`)  
This acts as a mathematical baseline, showing the absolute minimum number of page faults possible for a given trace.

\* \*\*Implementation Strategy:\*\*  
    \* \*\*Look-Ahead Engine:\*\* Unlike FIFO or LRU, the \`OPT\_Strategy\` class needs access to the entire page reference string loaded from the YAML config.  
    \* \*\*Eviction Logic:\*\* When a page fault occurs and physical frames are full, the algorithm scans the \*remainder\* of the reference string.   
    \* \*\*Selection:\*\* It calculates the "distance to next use" for every page currently in memory. It evicts the page with the longest distance (or the one that will never be used again).   
    \* \*\*Efficiency Score:\*\* At the end of the simulation, the system calculates \`(OPT Faults / User Algorithm Faults) \* 100\` to give the user an accuracy rating.

\#\#\# 4\. High-Fidelity Performance Telemetry (\`/utils/telemetry\_logger.cpp\`)  
This system records time-series data at every cycle, rather than just tallying final scores, enabling deep post-mortem analysis.

\* \*\*Implementation Strategy:\*\*  
    \* \*\*The Telemetry Struct:\*\* Create a \`CycleMetrics\` struct containing \`cycle\_id\`, \`active\_pid\`, \`cpu\_utilization\_percent\`, \`total\_page\_faults\`, and \`deadlocks\_avoided\`.  
    \* \*\*Cycle Polling:\*\* At the end of every tick in your main CPU loop, push a new \`CycleMetrics\` snapshot into a \`std::vector\<CycleMetrics\>\`.  
    \* \*\*CSV Exporter:\*\* On shutdown, a function iterates through the vector and writes the data to \`simulation\_results.csv\`, allowing the user to graph exactly when thrashing or bottlenecks occurred.  
