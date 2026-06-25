# DOCUMENT 02 — TRD (Technical Requirements Document)

# The exact tech stack — no deviations

---

## Language

C++20. No exceptions. std::counting\_semaphore requires C++20. Compile flag: \-std=c++20 CMakeLists.txt must set: set(CMAKE\_CXX\_STANDARD 20\) set(CMAKE\_CXX\_STANDARD\_REQUIRED ON) set(CMAKE\_CXX\_EXTENSIONS OFF)

## Build System

CMake 3.20 or higher. Generator: Ninja (faster than Make, use \-G Ninja). All dependencies fetched via CMake FetchContent — nothing installed system-wide except the compiler, cmake, ninja, git, and libncurses-dev.

## UI Library

FTXUI (Functional Terminal eXtension UI) by Arthur Sonzogni. Version: pinned to v5.0.0 via FetchContent GIT\_TAG. Source: [https://github.com/ArthurSonzogni/ftxui](https://github.com/ArthurSonzogni/ftxui) Components used: ftxui::component, ftxui::dom, ftxui::screen. FTXUI must run on the main thread only via screen.Loop(). Kernel must run on a dedicated std::thread and communicate via screen.PostEvent(). Never call any FTXUI rendering function from a background thread.

## Configuration Library

yaml-cpp by jbeder. Version: pinned to 0.8.0 via FetchContent GIT\_TAG. Source: [https://github.com/jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp) All simulation parameters loaded from config.yaml at startup. JSON is forbidden everywhere in this project. YAML only.

## Threading Primitives — All C++20 Standard Library

std::thread — one per simulated process \+ one for telemetry writer std::mutex — base synchronisation primitive std::shared\_mutex — for PCB fields read by UI, written by kernel std::unique\_lock — exclusive write access std::shared\_lock — concurrent read access (UI polling) std::counting\_semaphore — resource arbitration (Banker's resources) std::condition\_variable — scheduler wakeup (always use predicate overload) std::atomic\<bool\> — simulation\_running flag, shutdown coordination

## Memory Management

std::unique\_ptr — owning pointers throughout (no raw owning pointers) std::shared\_ptr — shared ownership where needed (e.g. PCB referenced by scheduler and concurrency manager simultaneously) Raw pointers — allowed only for non-owning observation (e.g. UI reading a PCB it does not own) RAII — mandatory for all locks, file handles, and thread lifetimes

## Key Data Structures

std::priority\_queue\<PCB\*, vector, Comparator\> — SRTF ready queue std::queue\<PCB\*\> — Round Robin ready queue std::vector\<int\> — physical frame array (VMM) std::unordered\_map\<int, int\> — page table (virtual → physical) std::unordered\_map\<int, vector\<ResourceToken\>\> — Banker's per-process holdings std::deque (ring buffer pattern) — telemetry cycle metrics buffer

## File I/O

std::ofstream — CSV telemetry writer (opened at startup, written incrementally) std::signal — SIGINT handler for graceful shutdown and CSV flush on Ctrl+C

## Folder Structure

os\_simulator/  
├── CMakeLists.txt  
├── config.yaml  
├── main.cpp  
├── include/  
│   ├── core/  
│   │   ├── process.h  
│   │   ├── cpu\_scheduler.h  
│   │   └── concurrency\_manager.h  
│   ├── memory/  
│   │   ├── mmu.h  
│   │   ├── page\_replacer.h  
│   │   ├── fifo\_strategy.h  
│   │   ├── lru\_strategy.h  
│   │   ├── clock\_strategy.h  
│   │   └── opt\_strategy.h  
│   ├── ui/  
│   │   ├── dashboard.h  
│   │   ├── memory\_view.h  
│   │   ├── lock\_monitor.h  
│   │   └── telemetry\_view.h  
│   └── utils/  
│       ├── config\_parser.h  
│       └── telemetry.h  
└── src/  
    ├── core/  
    │   ├── process.cpp  
    │   ├── cpu\_scheduler.cpp  
    │   └── concurrency\_manager.cpp  
    ├── memory/  
    │   ├── mmu.cpp  
    │   ├── fifo\_strategy.cpp  
    │   ├── lru\_strategy.cpp  
    │   ├── clock\_strategy.cpp  
    │   └── opt\_strategy.cpp  
    ├── ui/  
    │   ├── dashboard.cpp  
    │   ├── memory\_view.cpp  
    │   ├── lock\_monitor.cpp  
    │   └── telemetry\_view.cpp  
    └── utils/  
        ├── config\_parser.cpp  
        └── telemetry.cpp

## Naming Conventions

Classes : PascalCase (ProcessControlBlock, FifoStrategy) Methods : snake\_case (get\_snapshot(), step\_one\_cycle()) Member variables: snake\_case (remaining\_burst\_time\_, current\_priority\_) Member suffix : trailing underscore for private members Constants : ALL\_CAPS\_SNAKE (MAX\_FRAMES, DEFAULT\_QUANTUM) Files : snake\_case (cpu\_scheduler.cpp, fifo\_strategy.h) Namespaces : lowercase (namespace kernel, namespace memory, namespace ui)

## Hard Technical Constraints

1. Backend modules (/core/ and /memory/) must contain ZERO FTXUI includes or rendering logic. They only update internal state.  
2. Backend modules must not use std::cout during active simulation. All output goes through the TUI or the telemetry CSV.  
3. All shared state between kernel and UI must be protected by std::shared\_mutex.  
4. Global lock ordering must always be: ready\_queue\_mutex → KernelMutex → PCB rw\_mutex → telemetry\_mutex Never invert this order. Violation \= deadlock.  
5. Every condition\_variable::wait() call must use the predicate overload.  
6. No raw new/delete. Smart pointers only.  
7. YAML is the only configuration format. JSON is banned.

## Environment Variables

None required. All configuration is in config.yaml.

## Output Files

simulation\_results.csv — written to working directory on exit path configurable via config.yaml output\_csv\_path field  
