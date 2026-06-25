\# Backend Architecture & Kernel Specification

\#\# Overview  
This document outlines the core backend architecture for the C++ OS Simulator. The backend acts as the actual "Operating System Kernel," responsible for process lifecycle management, true multithreading, synchronization, and memory mapping. It is strictly decoupled from the Text User Interface (TUI), exposing thread-safe getter methods so the frontend can read the system state without interfering with kernel execution.

\---

\#\# 1\. The CPU & Process Scheduler (\`/core/\`)  
This subsystem is the central nervous system of the OS, dictating which simulated process gains access to the CPU based on configurable scheduling algorithms.

\* \*\*Process Control Block (PCB):\*\* A foundational data structure that tracks the lifecycle of every simulated program.  
  \* \*\*Attributes:\*\* Process ID (PID), Current State (Ready, Running, Blocked, Terminated), Base Priority, Remaining Burst Time, and a queue of memory page reference requests.  
\* \*\*Thread-Safe Ready Queue:\*\* A synchronized data structure (e.g., \`std::priority\_queue\` or \`std::queue\`) protected by mutexes, holding PCBs that are waiting for execution time.  
\* \*\*Configurable Dispatcher Engine:\*\* Evaluates the Ready Queue at every clock cycle and executes context switches based on the active algorithm:  
  \* \*\*Round Robin (RR):\*\* Implements a strict, configurable time-slice (quantum). Preempts the active thread when the quantum expires.  
  \* \*\*Shortest Remaining Time First (SRTF):\*\* A preemptive algorithm that continuously evaluates the pool and grants CPU time to the PCB with the lowest remaining burst time.

\#\# 2\. The Concurrency Engine (\`/core/\`)  
This subsystem manages real C++ hardware threads to simulate independent processes, enforcing strict synchronization to prevent memory corruption and system crashes.

\* \*\*Thread Pool Execution:\*\* Spawns and manages \`std::thread\` instances tied to active PCBs, simulating true parallel or concurrent execution environments.  
\* \*\*Resource Arbitration Layer:\*\* \* Implements wrappers around \`std::mutex\` and \`std::counting\_semaphore\`.  
  \* Forces threads into visual waiting queues when attempting to access a resource currently held in a critical section by another thread.  
\* \*\*Deadlock Avoidance (Banker's Algorithm):\*\* An active monitoring matrix. Before granting a semaphore lock, the kernel calculates the maximum possible future resource demands. If granting the lock results in an "Unsafe State" (potential deadlock), the request is dynamically denied or delayed.  
\* \*\*Priority Inheritance Protocol:\*\* Actively monitors for Priority Inversion. If a low-priority thread holds a lock required by a high-priority thread, the kernel temporarily boosts the lower thread's priority to accelerate execution and free the lock.

\#\# 3\. The Virtual Memory Manager (VMM) (\`/memory/\`)  
This component maps virtual memory requests from executing threads to a limited pool of simulated physical RAM.

\* \*\*Physical Frame Array:\*\* A fixed-size contiguous container (e.g., \`std::vector\<int\>\`) representing physical RAM hardware, divided into addressable page frames.  
\* \*\*The Page Table:\*\* A translation matrix mapping a process's virtual page numbers to physical frame indexes, alongside valid/invalid validation bits.  
\* \*\*Polymorphic Page Swapper:\*\* An object-oriented interface executing page replacement strategies when physical memory reaches capacity. Implementations include:  
  \* \*\*FIFO (First-In, First-Out):\*\* Evicts based on initial load timestamp.  
  \* \*\*LRU (Least Recently Used):\*\* Evicts based on an actively updated access timestamp.  
  \* \*\*Clock Algorithm:\*\* Utilizes a circular linked list and reference bits for optimized, low-overhead LRU approximation.  
  \* \*\*OPT (Belády's Optimal):\*\* A look-ahead evaluator used to calculate the theoretical mathematical minimum for page faults.

\#\# 4\. Data I/O & Telemetry (\`/utils/\`)  
This subsystem manages system initialization and the high-fidelity logging of kernel performance metrics.

\* \*\*YAML Initialization Engine:\*\* Utilizes the \`yaml-cpp\` library to parse \`config.yaml\` at boot. It injects parameters into the kernel, such as total physical frame count, time-slice quantum, and the list of incoming processes and their trace strings.  
\* \*\*Cycle-by-Cycle Logger:\*\* A background telemetry thread that records system snapshots at every CPU clock tick.  
  \* \*\*Metrics tracked:\*\* System-wide page fault ratio, isolated process page fault ratios, CPU utilization percentage, and the number of prevented deadlocks.  
\* \*\*Post-Mortem Exporter:\*\* Upon termination or simulated shutdown, it compiles the aggregated telemetry data into a structured \`.csv\` file, enabling the user to graph thrashing events or bottlenecks externally.

\---

\#\# Backend Technical Considerations  
\* \*\*Strict Decoupling:\*\* The backend kernel must contain \*\*zero\*\* UI or terminal rendering code. It solely updates internal states.  
\* \*\*Thread Safety (\`std::shared\_mutex\`):\*\* Because the TUI runs on a separate thread and constantly polls the backend for updates, all backend getter methods must use shared/read locks to prevent data races without halting the kernel's write operations.  
\* \*\*Error Handling:\*\* Invalid memory accesses (e.g., a process requesting a page outside its defined boundaries) should trigger a simulated "Segmentation Fault," gracefully terminating the specific PCB without crashing the actual C++ program.  
