\# Frontend Architecture & TUI Specification

\#\# Overview  
This document details the interactive frontend for the C++ OS Simulator. Unlike standard CLI applications that rely on sequential \`std::cout\` logging, this project utilizes a Text User Interface (TUI) powered by the \`FTXUI\` library. The frontend runs on a dedicated rendering thread, continuously polling the backend OS engine to display multi-panel, real-time visualizations of memory, concurrency, and scheduling states.

\---

\#\# 1\. Core Layout & Navigation (\`/ui/dashboard.cpp\`)  
The main dashboard divides the terminal into a structured grid to display simultaneous data streams without screen tearing or scrolling text.

\* \*\*Layout Structure:\*\*  
  \* \*\*Top Header:\*\* Displays system uptime, current CPU cycle, and active scheduling algorithm.  
  \* \*\*Left Column:\*\* The Concurrency Monitor and Active Process Control Block (PCB) Table.  
  \* \*\*Right Column:\*\* The Physical Memory Grid and live metrics.  
  \* \*\*Bottom Footer:\*\* The Interactive Command Palette.  
\* \*\*The Command Palette (Interactivity):\*\*  
  \* \`\[Spacebar\]\`: Steps the CPU forward by one clock cycle.  
  \* \`\[R\]\`: Resets the simulation to Cycle 0\.  
  \* \`\[Tab\]\`: Cycles focus between configurable parameters (e.g., dynamically adjusting time-slice quantums or memory frame limits on the fly).

\#\# 2\. The Physical Memory Grid (\`/ui/memory\_view.cpp\`)  
This component translates the underlying \`std::vector\` of physical RAM into a visual grid, making abstract page replacement algorithms intuitive.

\* \*\*Grid Visualization:\*\* Renders available physical page frames as fixed-size ASCII/ANSI boxes.  
\* \*\*Live Animation Mapping:\*\*  
  \* When a simulated process accesses a page, the requested page number appears inside a box.  
  \* \*\*Page Hit:\*\* The box border flashes \*\*Green\*\* to indicate the page was already resident.  
  \* \*\*Page Fault:\*\* The box border flashes \*\*Red\*\*, and the UI visually updates to show the evicted page being replaced according to the active algorithm (FIFO, LRU, Clock, or OPT).  
\* \*\*Validation Mode Display:\*\* When running a preset trace validation—such as feeding the exact sequence \`1, 2, 3, 4, 2, 3, 5, 6, 2, 1, 2, 3, 7, 6, 3, 3, 1, 2, 3, 6\`—a secondary timeline bar highlights the upcoming requests, allowing the user to anticipate evictions before hitting the Spacebar.

\#\# 3\. Concurrency Lock Monitor (\`/ui/lock\_monitor.cpp\`)  
This diagnostic panel visualizes the internal states of \`std::mutex\` and \`std::counting\_semaphore\` wrappers to demonstrate critical sections and the prevention of race conditions.

\* \*\*Semaphore Traffic Lights:\*\*  
  \* Renders system resources as labeled indicators.  
  \* \*\*\[GREEN O\]\*\*: Resource is available.  
  \* \*\*\[RED X\]\*\*: Resource is locked in a critical section. The panel explicitly prints the PID of the thread holding the lock.  
\* \*\*Visual Wait Queues:\*\*  
  \* Below each locked resource, a horizontal queue (e.g., \`Wait: \[PID 4\] \-\> \[PID 2\]\`) actively displays which threads are currently blocked by the operating system, waiting for the lock to be released.  
\* \*\*Priority Boost Indicator:\*\* If Priority Inheritance is triggered, the UI flashes an upward arrow \`\[^\]\` next to the PID holding the lock, indicating its priority has been temporarily elevated.

\#\# 4\. PCB Table & Real-Time Analytics (\`/ui/telemetry\_view.cpp\`)  
This view replaces standard log outputs with structured, dynamic tables and gauges.

\* \*\*Active PCB Table:\*\* A continuously updating matrix showing:  
  \* Process ID  
  \* Current State (Ready, Running, Blocked)  
  \* Base Priority vs. Current Priority  
  \* Remaining Burst Time  
\* \*\*Live Gauges:\*\*  
  \* \*\*Hit Rate Percentage:\*\* A horizontal progress bar that shifts left/right as the Page Hit/Fault ratio changes cycle by cycle.  
  \* \*\*CPU Utilization:\*\* A percentage gauge tracking how much time the CPU spends executing threads versus idling or handling context switches.

\#\# UI Technical Considerations  
\* \*\*Thread Safety:\*\* The UI must NEVER modify backend data directly. It should read from thread-safe getter methods protected by \`std::shared\_mutex\` to ensure rendering does not cause race conditions with the OS engine.  
\* \*\*YAML Integration:\*\* The initial layout constraints (number of boxes to draw for memory frames, names of the processes in the PCB table) must be dynamically generated at startup by parsing the \`config.yaml\` file, ensuring the UI perfectly scales to match the configuration.  
