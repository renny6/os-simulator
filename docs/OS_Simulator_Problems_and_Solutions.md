# OS Memory & Concurrency Simulator — Problems, Causes & Solutions

This document is a preemptive engineering brief. Every problem listed here has been
identified before a single line of code is written. When generating any file for this
project, you must read this document first and write your implementation to avoid each
issue described below.

---

## CATEGORY 1 — CONCURRENCY & THREADING

---

### C-01 | Data Race on Shared PCB State Between Kernel Thread and UI Renderer

**Severity:** Critical — guaranteed crash or undefined behaviour if not addressed.

**When it happens:**
The TUI renderer runs on its own thread and continuously polls PCB objects (reading
fields like `current_state`, `remaining_burst_time`, `base_priority`) to update the
dashboard. At the exact same time, the scheduler thread is writing to those same
fields as it performs context switches. This is a classic read-write race condition.
Even a single unsynchronised read of a multi-byte field can return a torn, half-written
value, producing garbage in the UI or, in the worst case, causing undefined behaviour
in the host process.

**How it manifests:**
- PCB table in the TUI shows impossible state values (e.g., negative burst time).
- Process state flickers between "Running" and "Ready" for no reason.
- Intermittent crashes that are impossible to reproduce reliably (the classic sign
  of a data race).

**Solution — implement exactly this pattern:**
Protect every PCB field that the UI reads using `std::shared_mutex`. The scheduler
thread acquires a `std::unique_lock` (exclusive write lock) when modifying PCB state.
The UI thread acquires a `std::shared_lock` (shared read lock) when reading. Multiple
UI reads can happen simultaneously; a scheduler write blocks all readers until it
completes.

The preferred pattern is to define a lightweight `PCBSnapshot` struct containing only
the fields the UI needs (state, burst_time, priority, pid). The scheduler writes a
complete snapshot atomically under a unique_lock. The UI clones the snapshot under a
shared_lock and releases the lock immediately. This minimises lock contention because
the UI never holds the lock while doing rendering work.

```
// Correct pattern:
struct PCBSnapshot { int pid; ProcessState state; int remaining_burst; int priority; };

class ProcessControlBlock {
    mutable std::shared_mutex rw_mutex;
    PCBSnapshot snapshot;
public:
    // Scheduler calls this (exclusive write):
    void update_state(ProcessState s) {
        std::unique_lock lock(rw_mutex);
        snapshot.state = s;
    }
    // UI calls this (shared read):
    PCBSnapshot get_snapshot() const {
        std::shared_lock lock(rw_mutex);
        return snapshot; // copy under lock, render outside lock
    }
};
```

Never expose raw getter methods that read PCB fields without acquiring the lock first.

---

### C-02 | Deadlock Between the Ready-Queue Mutex and the KernelMutex Wrapper

**Severity:** Critical — produces a freeze in the host process that requires a force-kill.

**When it happens:**
This is a self-inflicted deadlock caused by incorrect lock ordering. It happens when:
- Thread A (the scheduler) holds the `ready_queue_mutex` and calls into the
  `ConcurrencyManager` to grant a simulated resource lock.
- Inside the ConcurrencyManager, the priority inheritance logic tries to re-acquire
  the `ready_queue_mutex` to re-sort the queue after boosting a thread's priority.
- Both locks now wait for each other. The host process freezes permanently.

This is one of the most common bugs in systems that have multiple mutexes.

**How it manifests:**
- The simulator appears to run for a few cycles then completely hangs.
- The TUI stops updating. The process uses 0% CPU.
- Only a SIGKILL can terminate it.

**Solution — establish and enforce a global lock ordering:**
Define a strict, documented hierarchy of every mutex in the project. Every thread
in the entire codebase must acquire locks in this order, without exception:

```
Lock Acquisition Order (always acquire in this sequence):
  1. ready_queue_mutex        (outermost)
  2. KernelMutex (per-resource)
  3. PCB rw_mutex (per-process)
  4. telemetry_mutex          (innermost)
```

No code path is ever permitted to acquire a lock that is higher in this list while
holding one that is lower. When two locks at the same level must be acquired
simultaneously (e.g., two KernelMutexes), use `std::lock(m1, m2)` which avoids
circular waits by acquiring both atomically.

If the priority inheritance logic needs to modify the queue, it must do so by posting
a deferred update (via a flag or a notification queue) that the scheduler processes
on its own next iteration — it must never call back into the scheduler while holding
a lower-level lock.

---

### C-03 | Priority Boost Not Reverted After KernelMutex Is Released

**Severity:** Critical — produces incorrect scheduling behaviour that is silent and hard to detect.

**When it happens:**
Thread B (low priority) holds a KernelMutex. Thread A (high priority) is blocked
waiting for it. The ConcurrencyManager temporarily boosts Thread B's priority to
match Thread A so B finishes faster and releases the lock. The bug occurs when
Thread B releases the lock but its priority is not immediately reverted back to its
original base priority before the scheduler re-evaluates the ready queue. Thread B
continues running at elevated priority, starving other medium-priority threads and
corrupting the intended scheduling order.

**How it manifests:**
- After a priority inheritance event, the previously-low-priority thread keeps
  dominating CPU time unexpectedly.
- The PCB table in the TUI shows a process with a "Current Priority" higher than its
  "Base Priority" for many cycles after it should have reverted.
- Scheduling trace does not match theoretical SRTF or RR expectations.

**Solution — use RAII to guarantee the revert:**
Define a `PriorityGuard` RAII object. When the ConcurrencyManager boosts a thread's
priority, it creates a `PriorityGuard` for that thread. The guard stores the thread's
original base priority. When the guard goes out of scope (i.e., when the lock is
released), its destructor automatically reverts the priority. This makes it impossible
to forget the revert — the C++ destructor guarantees it runs regardless of how the
lock is released (normal flow, exception, early return).

```
class PriorityGuard {
    ProcessControlBlock& pcb;
    int original_priority;
public:
    PriorityGuard(ProcessControlBlock& p, int boosted_to)
        : pcb(p), original_priority(p.get_base_priority()) {
        pcb.set_current_priority(boosted_to);
    }
    ~PriorityGuard() {
        pcb.set_current_priority(original_priority); // always runs
    }
};
```

The KernelMutex::unlock() must trigger the destruction of this guard. The scheduler
must be notified (via condition_variable) to re-evaluate the queue after the revert.

---

### C-04 | Spurious Wakeups in Condition Variables Causing Premature Scheduling

**Severity:** High — causes incorrect context switches and corrupts scheduling order.

**When it happens:**
C++ condition variables can wake up spuriously — the thread wakes without any
`notify_one()` or `notify_all()` being called. This is permitted by the C++ standard
and happens due to OS-level signal handling. If the scheduler acts on every wakeup
without checking whether the actual condition is true, it triggers ghost context
switches at wrong times, processes get dispatched when they shouldn't be, and the
scheduling trace becomes non-deterministic.

**How it manifests:**
- Processes are dispatched out of order in Round Robin.
- Clock cycle count mismatches expected behaviour.
- The simulation is non-reproducible even with the same config.yaml.

**Solution — always use the predicate overload of wait():**
Never use the bare `cv.wait(lock)` form. Always pass a predicate lambda that
re-checks the actual condition. The standard library guarantees this will loop back
to sleep if the predicate returns false, even after a wakeup.

```
// WRONG — vulnerable to spurious wakeup:
cv.wait(lock);

// CORRECT — predicate guards against spurious wakeup:
cv.wait(lock, [&]{ return !ready_queue.empty() || !simulation_running; });
```

Every condition_variable::wait() call in the entire project must use this predicate form.

---

### C-05 | Thread Starvation in SRTF — Long-Burst Processes Never Run

**Severity:** High — produces a correct-but-misleading simulation state that looks like a bug.

**When it happens:**
SRTF is a preemptive algorithm that always grants the CPU to the process with the
smallest remaining burst time. If short-burst processes arrive continuously, a
long-burst process can wait in the ready queue indefinitely and never get scheduled.
The PCB table in the TUI will show it stuck in "Ready" for hundreds of cycles, which
every observer will assume is a deadlock or bug.

**How it manifests:**
- One PCB row shows "Ready" state with an unchanged burst time for the entire simulation.
- Users file a bug report. There is no bug — this is correct SRTF behaviour.
- Without a starvation counter, there is no way to explain what the user is seeing.

**Solution — implement aging and expose the starvation counter in the TUI:**
Add a `starvation_counter` field to the PCB. Increment it for every clock cycle a
process spends in the "Ready" state without being dispatched. Every N cycles (N is
configurable in config.yaml), increase the process's effective scheduling priority by
1 to prevent permanent starvation. This is standard aging as described in Tanenbaum's
Modern Operating Systems.

Display the starvation_counter in the PCB table in the TUI alongside a tooltip or
label that explains the phenomenon. This turns a confusing symptom into a visible,
educational feature of the simulation. Log starvation events in the telemetry CSV.

---

### C-06 | std::counting_semaphore Underflow if release() Is Called More Times Than acquire()

**Severity:** Rare — only occurs if a process is terminated abnormally while holding semaphore tokens.

**When it happens:**
A simulated process dies mid-execution (e.g., it accesses an out-of-bounds virtual
address and is terminated). The cleanup code calls `semaphore.release()` on its behalf
to return its held tokens. If the cleanup code has a bug that calls `release()` twice
(perhaps once in a destructor and once in an explicit cleanup path), the semaphore
internal counter exceeds its construction-time maximum. The C++ standard defines this
as undefined behaviour — the counter wraps or crashes depending on implementation.

**How it manifests:**
- A semaphore that should be at maximum capacity reports it has more tokens than it
  was constructed with.
- Subsequent `acquire()` calls succeed when they should block, allowing more processes
  into a critical section than intended.
- The Lock Monitor in the TUI shows a resource as "Available" when it should be locked.

**Solution — track semaphore ownership per-process in ConcurrencyManager:**
Maintain a `std::unordered_map<int, std::vector<ResourceToken>> process_holdings`
inside ConcurrencyManager, keyed by PID. Every time a process acquires a semaphore
token, record it. Every time a process releases, remove it from the record. On
abnormal process termination, the ConcurrencyManager iterates this map, releases
exactly the tokens that process holds (no more), then removes the entry. Add an
assertion: `assert(semaphore_count <= max_count)` after every release, to catch this
in debug builds immediately.

---

## CATEGORY 2 — VIRTUAL MEMORY MANAGER

---

### M-01 | Clock Algorithm Pointer Goes Out-of-Bounds After Runtime Frame Resize

**Severity:** Critical — produces an out-of-bounds vector access (UB or segfault).

**When it happens:**
The TUI spec allows the user to adjust the physical frame count at runtime via a
slider. The Clock (Second-Chance) algorithm maintains a `clock_ptr` integer that
advances through the frame array circularly. When the user drags the slider and
reduces the frame count, the existing `clock_ptr` value may now point past the end
of the newly resized `frames` vector. The very next time the Clock algorithm tries to
advance the pointer, it accesses an invalid index.

**How it manifests:**
- Immediate segfault or access violation the first time a page fault occurs after
  the user drags the frame slider to a smaller value.
- Undefined behaviour: the algorithm may read garbage data from memory beyond the
  vector, produce wrong eviction decisions, and never crash visibly.

**Solution — always compute the pointer modulo current size, and reset on resize:**
Advance the clock pointer exclusively as: `clock_ptr = (clock_ptr + 1) % frames.size()`.
This is not a sufficient fix on its own if `frames.size()` changes mid-advance.

The critical fix: hold a write lock on the VMM during any frame array resize. After
resizing (whether growing or shrinking), immediately reset `clock_ptr = 0`. The loss
of the circular position is acceptable — the alternative is UB. The TUI slider must
post the resize as a command to a thread-safe queue that the VMM processes on its
next tick, never directly modifying the frame array from the UI thread.

---

### M-02 | OPT Algorithm Produces Wrong Eviction Decisions in a Multi-Process Simulation

**Severity:** Critical — produces incorrect page fault counts that invalidate the efficiency score.

**When it happens:**
Belady's Optimal (OPT) algorithm needs to look ahead in the page reference string to
find which page currently in memory will be used furthest in the future (or never),
and evicts that one. This is straightforward for a single process with one linear
reference string. In this simulator, multiple processes share the VMM and each has
its own reference string. Their accesses interleave based on the scheduler. A naïve
OPT implementation that treats all processes' strings as a single merged sequence
will scan the wrong string, evict the wrong page, and produce a fault count that is
mathematically incorrect — making the OPT efficiency score meaningless.

**How it manifests:**
- The OPT efficiency percentage does not match academic reference values.
- On the validation trace (the spec's reference sequence with known fault counts),
  OPT produces the wrong number.
- The comparison between user algorithms and OPT is unreliable.

**Solution — OPT must operate per-process, not globally:**
Each process's `OPT_Strategy` instance receives only that process's own reference
string from the YAML config. When a page fault occurs for Process X, OPT scans only
Process X's remaining reference string to determine its eviction decision. The global
OPT efficiency score is computed as the weighted average of per-process efficiency
scores, weighted by each process's total access count. Never merge reference strings
across processes into a single OPT lookahead.

---

### M-03 | LRU Timestamp Collision When Two Accesses Occur in the Same Clock Cycle

**Severity:** High — produces non-deterministic eviction decisions that cannot be validated.

**When it happens:**
The LRU algorithm evicts the page with the oldest "last used" timestamp. If the
timestamp is based on the clock cycle counter (a single integer that increments once
per tick), two pages accessed in the same tick get identical timestamps. LRU has no
tiebreaker defined. The eviction choice becomes arbitrary — whichever page happens
to appear first or last in the frame array gets evicted. This means the simulation
produces different results on different runs with identical config, making the
validation suite (which checks against known fault counts) unreliable.

**How it manifests:**
- LRU fault count varies across runs of the same trace.
- Validation against reference sequences fails intermittently.
- The post-mortem CSV shows the same fault count varying by 1-2 between runs.

**Solution — use a monotonic access counter, not a clock cycle counter:**
Define a `uint64_t access_counter` that is a member of the VMM. Increment it on
every single page access, not once per cycle. Assign the current value of
`access_counter` as the LRU timestamp whenever a page is accessed. Because the
counter increments atomically per-access (not per-cycle), every access gets a
strictly unique timestamp, and LRU always has a deterministic tiebreaker.

---

### M-04 | Belady's Anomaly Looks Like a Bug When the User Increases Frame Count

**Severity:** High — produces user confusion and misleading bug reports.

**When it happens:**
FIFO is susceptible to Belady's Anomaly: for certain reference strings, increasing
the number of physical frames paradoxically increases the number of page faults. This
is a mathematically proven property of FIFO and is not a bug. However, when the user
drags the TUI frame slider upward and observes the fault count jumping up instead of
down, they will immediately assume the implementation is broken.

**How it manifests:**
- User increases frames from 3 to 4, FIFO fault count increases from 9 to 10.
- User files a bug report: "Adding more memory causes more faults — clearly broken."
- No amount of debugging will "fix" it because it is correct behaviour.

**Solution — detect and label Belady's Anomaly explicitly in the TUI:**
The VMM should track the fault count at each frame count value tested during the
session. After a frame count increase, if the new fault count is greater than the
previous fault count, set a `belady_anomaly_detected` flag. The memory panel in the
TUI should display a clearly labelled indicator: "Belady's Anomaly: FIFO fault count
increased with more frames — this is expected and correct for this reference string."
Log a `belady_anomaly` event in the telemetry CSV. Turn a bug report into a teaching
moment.

---

### M-05 | Thrashing Detection Fires During the Cold-Start Phase

**Severity:** Medium — causes the simulator to incorrectly suspend processes at startup.

**When it happens:**
The spec requires monitoring page-fault frequency and suspending low-priority
processes if the system spends more time paging than executing. At simulation startup,
every page in every process's reference string is cold — not in any physical frame.
The fault rate for the first N accesses is 100% by definition. If thrashing detection
is active from cycle 0, the fault frequency threshold is immediately exceeded, the
system declares thrashing, and low-priority processes are suspended before they have
ever had a chance to run. The simulation becomes stuck before it even begins.

**How it manifests:**
- Low-priority processes are immediately marked "Blocked" at cycle 1.
- The simulation appears frozen; only high-priority processes ever run.
- Disabling thrashing mitigation "fixes" the issue — confirming the cold-start
  window is the culprit.

**Solution — enforce a warm-up window before thrashing detection activates:**
Define a `warmup_cycles` constant, defaulting to `num_frames * 2`. Do not evaluate
the thrashing condition during the first `warmup_cycles` accesses. After the
warm-up period, begin computing the rolling fault frequency and apply the threshold.
Display the warm-up phase visually in the TUI (e.g., a "Warming up..." label on the
memory panel). Log the end of the warm-up phase in the telemetry CSV so the user
can see when active monitoring began.

---

## CATEGORY 3 — FTXUI / TUI FRONTEND

---

### U-01 | FTXUI ScreenInteractive::Loop() Blocks the Main Thread — Kernel Cannot Run

**Severity:** Critical — the simulator either renders but doesn't simulate, or simulates but doesn't render.

**When it happens:**
`ftxui::ScreenInteractive::Loop()` is a blocking call. It does not return until the
UI is closed. If both the kernel loop and the FTXUI loop are placed on the same
thread (the main thread), one of them runs and the other never starts. This is the
single most common mistake made when integrating FTXUI with any background work.

**How it manifests:**
- The TUI renders the initial state perfectly but nothing ever updates.
- The clock cycle counter never increments.
- Pressing spacebar has no effect because the kernel is not running.

**Solution — run the kernel on a dedicated std::thread:**
Launch the kernel simulation loop on a `std::thread` before calling
`screen.Loop()`. Pass a reference to the `ScreenInteractive` object into the kernel
thread so it can call `screen.PostEvent(ftxui::Event::Custom)` after each tick to
trigger a re-render. Never call FTXUI rendering or component functions from the
background thread — only `PostEvent` is thread-safe. The main thread runs
`screen.Loop()` exclusively.

```
// Correct startup pattern in main.cpp:
auto screen = ftxui::ScreenInteractive::Fullscreen();
std::thread kernel_thread([&]() {
    while (simulation_running) {
        kernel.tick();
        screen.PostEvent(ftxui::Event::Custom); // trigger repaint
        std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms));
    }
});
screen.Loop(dashboard_component);
kernel_thread.join();
```

---

### U-02 | UI Accesses Freed Kernel Objects During Teardown (Use-After-Free)

**Severity:** Critical — causes a crash or silent memory corruption on exit.

**When it happens:**
When the user presses the exit key or closes the terminal, the main thread exits
`screen.Loop()` and begins destroying kernel objects. If the kernel thread has not
been joined yet, and if FTXUI fires one final repaint event after destruction begins,
the rendering callbacks will attempt to call getter methods on PCB objects that have
already been freed. This is a use-after-free — undefined behaviour that may produce
a crash, garbage output, or silent data corruption.

**How it manifests:**
- Occasional crash specifically on exit, not during the simulation itself.
- Valgrind or AddressSanitizer reports invalid read after free on exit.
- The crash is non-reproducible — it depends on thread scheduling timing.

**Solution — use an atomic shutdown flag and join before destroying:**
Declare `std::atomic<bool> simulation_running{true}` as a shared variable. On exit,
set it to false before doing anything else. All UI getter methods must check this
flag first and return safe empty/default values if it is false. After `screen.Loop()`
returns, call `kernel_thread.join()` before any kernel objects go out of scope or
are destroyed. This guarantees the kernel thread has fully stopped before the objects
it uses are freed.

---

### U-03 | Memory Grid Overflows on Narrow Terminals

**Severity:** High — breaks the dashboard layout on any terminal below ~120 columns wide.

**When it happens:**
The memory grid renders one box per physical frame. The number of frames is loaded
from config.yaml. If config.yaml specifies 16 frames and the user's terminal is 80
columns wide, the grid overflows and wraps onto new lines, completely destroying the
panel layout. FTXUI does not automatically truncate or reflow grid content.

**How it manifests:**
- Memory grid boxes wrap and overlap other panels.
- The lock monitor and PCB table are pushed off-screen.
- The dashboard looks completely broken on any terminal smaller than expected.

**Solution — query terminal size and cap boxes per row dynamically:**
At startup and on every resize event, query the terminal dimensions using
`ftxui::Terminal::Size()`. Calculate the maximum number of boxes that fit in one row:
`boxes_per_row = floor(available_width / (box_width + gap))`. If the frame count from
config.yaml exceeds this, wrap into multiple rows. FTXUI fires resize events
automatically — subscribe to them and recalculate the grid layout on each one.
Add a minimum terminal size check on startup: if the terminal is below 100 columns,
display a clear error and exit gracefully rather than rendering a broken layout.

---

### U-04 | Page Hit/Fault Flash Animation Does Not Reset Between Frames

**Severity:** Medium — produces a permanently colored memory grid that is visually incorrect.

**When it happens:**
The spec requires page hit boxes to flash green and page fault boxes to flash red.
If the flash state is stored as a simple boolean (`is_faulted = true`) and is set but
never cleared, every frame that has ever experienced a fault stays permanently red.
After a few cycles, the entire grid is red or green, making it impossible to see
individual events.

**How it manifests:**
- After the first few page accesses, all grid boxes are the same color permanently.
- New hit/fault events are invisible because the grid is already fully colored.
- The TUI provides no useful visual feedback after the first few cycles.

**Solution — use a per-frame countdown counter for flash state:**
Instead of a boolean, store a `flash_countdown` integer per frame slot. When a hit
or fault occurs, set `flash_countdown = 3` (or a configurable value representing
render frames to display the flash). In each render pass, if `flash_countdown > 0`,
apply the flash color and decrement it. When it reaches 0, revert to the neutral
color. The kernel sets the countdown; the UI reads and decrements it under the shared
lock. This produces a short, visible flash that naturally fades without any timer thread.

---

## CATEGORY 4 — BANKER'S ALGORITHM

---

### B-01 | Banker's Algorithm Safety Check Runs Every Clock Cycle — Performance Collapse

**Severity:** Critical — makes the simulator unresponsive at moderate process counts.

**When it happens:**
The classic Banker's Algorithm safety check has O(n²) time complexity relative to
process count (n). If the check runs on every single clock cycle tick rather than
only when a resource is requested, the cost per tick scales quadratically. With 20
processes and a fast tick rate, this can burn more CPU time on safety checks than
on the simulation itself, causing the TUI to stutter and the clock to advance slowly.

**How it manifests:**
- The simulator runs fine with 3-5 processes and slows dramatically with 10+.
- CPU usage of the host process stays at 100% even during "idle" simulation cycles.
- The TUI frame rate drops as process count increases.

**Solution — run the safety check only on resource requests, not every tick:**
The Banker's safety check must be triggered exclusively when a process submits a
resource request to the ConcurrencyManager. It must not run on every scheduler tick.
Cache the last known safe state. Invalidate the cache only when the allocation matrix
changes (i.e., when a resource is granted or released). Between resource events, use
the cached safe/unsafe state. At the scale of this simulation (under 20 processes),
this reduces the per-tick overhead to near zero.

---

### B-02 | Banker's Algorithm Requires Max-Claim Values Not Present in config.yaml

**Severity:** High — algorithm cannot function without this data; failure is silent or misleading.

**When it happens:**
Dijkstra's Banker's Algorithm requires every process to declare its maximum possible
resource needs before execution begins (the "max claim" matrix). If the config.yaml
process entries do not include a `max_resources` field, the algorithm has no max claim
to work with. A naïve implementation will either crash when it tries to read the field,
silently use 0 (which makes every state appear unsafe), or skip deadlock avoidance
entirely.

**How it manifests:**
- Every resource request is denied as "unsafe" even when the system is safe.
- The simulation runs with zero concurrency because no resource is ever granted.
- Or: a KeyError / missing-field exception terminates the config parser.

**Solution — require max_resources in config.yaml and handle the missing-field case:**
Add a `max_resources` list field to every process entry in config.yaml. The config
parser must validate its presence on startup and exit with a clear error if missing.
As a fallback for operator convenience, if `max_resources` is absent for a process,
default it to the total system resource count (worst case, most conservative). Log a
warning in telemetry when this default is applied so the user knows the safety check
is operating conservatively rather than with accurate data.

---

## CATEGORY 5 — BUILD SYSTEM & DEPENDENCIES

---

### D-01 | CMake Cannot Find yaml-cpp or FTXUI on a Fresh Machine

**Severity:** Critical — the project does not build at all on any machine other than the dev machine.

**When it happens:**
On Linux, system-installed yaml-cpp may be in `/usr/local/` or a user prefix that
CMake's default search paths do not include. On macOS with Homebrew, libraries land
in `/opt/homebrew/` which CMake also does not search by default. On Windows, neither
library is likely installed at all. A CMakeLists.txt that uses `find_package()` alone
will fail on any machine that hasn't been manually configured — which is every CI
runner and every collaborator's machine.

**How it manifests:**
- `CMake Error: Could not find a configuration file for "yaml-cpp"` on configure.
- Build fails before a single source file is compiled.
- Works on one machine, fails on all others.

**Solution — use CMake FetchContent to vendor both dependencies:**
Replace `find_package()` for both FTXUI and yaml-cpp with `FetchContent_Declare`
and `FetchContent_MakeAvailable`. This downloads and builds the exact pinned version
at configure time, eliminating all path and version ambiguity.

```cmake
include(FetchContent)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v5.0.0
)

FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp
  GIT_TAG 0.8.0
)

FetchContent_MakeAvailable(ftxui yaml-cpp)

target_link_libraries(os_simulator PRIVATE
    ftxui::component ftxui::dom ftxui::screen yaml-cpp)
```

Pin exact Git tags — never use HEAD or a branch name, as upstream changes will
break the build unpredictably.

---

### D-02 | std::counting_semaphore Unavailable When Compiler Defaults to C++17

**Severity:** High — produces a cryptic compile error that looks like a missing library.

**When it happens:**
`std::counting_semaphore` is defined in `<semaphore>`, which is a C++20 header.
Many Linux distributions ship GCC with a default standard of C++14 or C++17. If the
CMakeLists.txt does not explicitly enforce C++20, the compiler uses its default, the
`<semaphore>` header is not found, and the error message ("no member named
'counting_semaphore' in namespace 'std'") looks to a new developer like a missing
library rather than a standard version issue.

**How it manifests:**
- Compile error: `error: 'counting_semaphore' is not a member of 'std'`.
- Developer spends time looking for a semaphore library to install.
- Correct fix takes 1 line in CMakeLists.txt but is not obvious.

**Solution — enforce C++20 and REQUIRED in CMakeLists.txt:**
Add these two lines to CMakeLists.txt, before any target definitions:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

The `REQUIRED ON` setting makes CMake fail at configure time with a clear message
if the compiler does not support C++20, rather than silently falling back. Also add
`cmake_minimum_required(VERSION 3.20)` at the top of CMakeLists.txt — CMake 3.20+
has reliable C++20 support across platforms.

---

### D-03 | FTXUI Keyboard Events Fire Twice on Windows Terminal

**Severity:** Rare — only affects Windows users and only on specific terminal versions.

**When it happens:**
On older versions of Windows Terminal and on cmd.exe, ANSI escape sequence handling
differs from Linux/macOS. Specifically, certain key events (spacebar, tab, enter) can
be dispatched twice in rapid succession — once as the raw keycode and once as the
processed character. FTXUI receives both events. If the spacebar advances the
simulation by one cycle per press, accidental double-dispatch advances it by two,
which breaks step-by-step debugging.

**How it manifests:**
- Pressing spacebar once advances two clock cycles instead of one.
- Only reproducible on Windows; Linux and macOS are unaffected.
- Tab focus cycling skips panels.

**Solution — debounce keyboard events with a timestamp check:**
In the FTXUI event handler, record the timestamp of the last processed event using
`std::chrono::steady_clock`. On every incoming event of the same type, check if fewer
than 50 milliseconds have elapsed since the last identical event. If so, discard it
as a duplicate. 50ms is imperceptible to a human but far longer than the double-
dispatch window.

```cpp
auto last_space = std::chrono::steady_clock::now();
// In event handler:
if (event == Event::Character(' ')) {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_space).count() > 50) {
        last_space = now;
        kernel.step_one_cycle();
    }
}
```

Document in README.md that Windows Terminal version 1.12 or later is the minimum
supported terminal on Windows.

---

## CATEGORY 6 — TELEMETRY & CSV EXPORT

---

### T-01 | Telemetry Vector Grows Without Bound — OOM on Long Simulations

**Severity:** High — causes the host process to be killed by the OS on long-running simulations.

**When it happens:**
The spec requires pushing a `CycleMetrics` snapshot into a `std::vector` on every
clock tick. Each snapshot is a struct with several integer fields. On a simulation
running for 10,000 cycles at a moderate struct size of ~100 bytes, this vector
reaches ~1 MB. At 1 million cycles (achievable by leaving the simulator running),
it reaches ~100 MB. The vector's continuous growth eventually triggers OS-level
memory pressure and either OOM-kills the process or degrades system performance.

**How it manifests:**
- Simulator runs fine for the first few minutes then slows and is killed.
- The crash is not related to any simulation logic — it is pure memory exhaustion.
- The telemetry CSV is never written because the process is killed before shutdown.

**Solution — write telemetry to disk in a background thread instead of buffering in RAM:**
Create a dedicated telemetry writer thread that owns an `std::ofstream` opened in
the output CSV file from startup. The kernel pushes `CycleMetrics` structs into a
thread-safe ring buffer (size: 1000 entries). The writer thread drains the ring
buffer and writes directly to disk, then flushes. This caps telemetry memory usage
at a fixed ring buffer size regardless of simulation length. The CSV is written
incrementally and is always valid even if the process is killed mid-run.

---

### T-02 | CSV File Is Corrupt or Truncated If Simulation Is Killed with Ctrl+C

**Severity:** Medium — destroys the post-mortem data the user needs for analysis.

**When it happens:**
If the user sends SIGINT (Ctrl+C) while the simulation is running, the process
receives the signal and terminates immediately. The `std::ofstream` for the CSV is
destroyed without being flushed. The file on disk is either empty, missing the
header row, or truncated mid-line. The file is malformed and cannot be opened in
Excel or any analysis tool.

**How it manifests:**
- After Ctrl+C, the CSV file exists but is 0 bytes, or is cut off at an arbitrary line.
- Excel fails to parse the file.
- The user loses all telemetry data from the run.

**Solution — register a SIGINT handler that triggers graceful shutdown:**
In main.cpp, register a signal handler using `std::signal(SIGINT, handler)`. The
handler sets `simulation_running = false`. The kernel loop checks this flag and exits
cleanly. The telemetry writer thread is then joined, which ensures all buffered
entries are flushed to the CSV file before the ofstream is closed. Use RAII on the
ofstream so its destructor (which calls `close()` and `flush()`) is guaranteed to run.

```cpp
std::signal(SIGINT, [](int) { simulation_running.store(false); });
```

The CSV header row must be written at file open time, not at the first data write,
so even a very early kill produces a valid (if short) CSV file.

---

## CATEGORY 7 — ALGORITHMIC CORRECTNESS

---

### A-01 | Round Robin Creates Ghost PCB Entry When Process Finishes Exactly on Quantum Expiry

**Severity:** Critical — corrupts the ready queue with a zombie process entry.

**When it happens:**
In Round Robin, the scheduler checks two conditions at the end of each tick:
1. Has the current process's remaining burst time reached 0? (Process done.)
2. Has the time quantum expired? (Preempt and re-queue.)

If a process's remaining burst time reaches exactly 0 on the same tick the quantum
expires, a naïve implementation triggers both conditions and re-enqueues the process
back into the ready queue with 0 remaining burst time before terminating it. The
next time this "ghost" PCB is dispatched, it runs for 0 cycles, generates a zero-
duration telemetry entry, and may cause a division-by-zero or infinite loop if the
scheduler divides by remaining burst time.

**How it manifests:**
- A terminated process reappears in the PCB table as "Ready" with 0 burst time.
- The simulation hangs after a certain number of processes complete.
- Telemetry CSV contains anomalous entries with 0-duration cycles.

**Solution — check process completion before checking quantum expiry:**
The order of checks at the end of each tick must be strictly:

```
1. Decrement remaining_burst_time.
2. If remaining_burst_time == 0:
       Mark PCB as Terminated.
       Remove from active processes.
       Do NOT check quantum. Stop.
3. Else if quantum_counter >= time_slice_quantum:
       Reset quantum_counter.
       Move PCB to back of ready queue.
```

The completion check unconditionally takes priority. Once a process is terminated,
no further scheduling logic runs for it in that tick.

---

### A-02 | FIFO Validation Trace Produces Wrong Fault Count Due to Cold-Start Off-by-One

**Severity:** Rare — causes the built-in validation suite to fail on the reference sequence.

**When it happens:**
The spec defines a validation trace: `1, 2, 3, 4, 2, 3, 5, 6, 2, 1, 2, 3, 7, 6,
3, 3, 1, 2, 3, 6`. With 3 physical frames, the correct FIFO fault count is 15. A
common off-by-one mistake is to treat the initial loading of pages into empty frames
as "hits" (because no eviction occurs) rather than as faults. This produces a fault
count of 12 (only counting the 12 accesses where eviction occurred) instead of the
correct 15.

**How it manifests:**
- The automated verification suite reports FIFO as "incorrect" with 12 faults instead
  of the expected 15.
- Manually tracing through the sequence confirms the implementation is doing correct
  FIFO eviction — the bug is purely in what counts as a fault.

**Solution — count every miss as a fault, including initial cold loads:**
A page fault occurs every time a requested page is not currently resident in any
physical frame. This includes the first access to any page — even when frames are
still empty and no eviction is necessary. There are no "free" loads.

The fault counter must increment whenever: `page not in any frame`, regardless of
whether a frame slot was empty or an eviction was required. The first 3 accesses in
the trace (pages 1, 2, 3) each produce a fault. Access to page 4 also produces a
fault and triggers the first FIFO eviction. This gives 15 total faults for the
reference trace with 3 frames, which is the standard textbook answer.

---

## SUMMARY TABLE

| ID   | Category          | Severity | Brief Description                                   |
|------|-------------------|----------|-----------------------------------------------------|
| C-01 | Concurrency       | Critical | Data race on PCB fields between kernel and UI thread |
| C-02 | Concurrency       | Critical | Self-inflicted deadlock from incorrect lock ordering |
| C-03 | Concurrency       | Critical | Priority boost not reverted on mutex release         |
| C-04 | Concurrency       | High     | Spurious wakeup triggers premature scheduling        |
| C-05 | Concurrency       | High     | SRTF starvation looks like a deadlock in the TUI     |
| C-06 | Concurrency       | Rare     | Semaphore underflow on abnormal process termination  |
| M-01 | Memory Manager    | Critical | Clock pointer out-of-bounds after frame slider resize|
| M-02 | Memory Manager    | Critical | OPT produces wrong faults in multi-process context   |
| M-03 | Memory Manager    | High     | LRU timestamp collision within same clock cycle      |
| M-04 | Memory Manager    | High     | Belady's Anomaly misread as a FIFO implementation bug|
| M-05 | Memory Manager    | Medium   | Thrashing detection fires during cold-start phase    |
| U-01 | TUI / FTXUI       | Critical | FTXUI loop blocks main thread; kernel never ticks    |
| U-02 | TUI / FTXUI       | Critical | UI accesses freed kernel objects during teardown     |
| U-03 | TUI / FTXUI       | High     | Memory grid overflows on narrow terminal windows     |
| U-04 | TUI / FTXUI       | Medium   | Flash animation stays permanently on after first hit |
| B-01 | Banker's Algorithm| Critical | Safety check runs every tick; O(n²) kills performance|
| B-02 | Banker's Algorithm| High     | max_resources field missing from config.yaml         |
| D-01 | Build System      | Critical | CMake cannot find FTXUI or yaml-cpp on fresh machine |
| D-02 | Build System      | High     | C++17 default breaks std::counting_semaphore include |
| D-03 | Build System      | Rare     | FTXUI keyboard events double-fire on Windows Terminal|
| T-01 | Telemetry         | High     | Telemetry vector grows unbounded; OOM on long runs   |
| T-02 | Telemetry         | Medium   | CSV truncated or empty if killed with Ctrl+C         |
| A-01 | Algorithms        | Critical | Round Robin creates ghost PCB on quantum-exact finish |
| A-02 | Algorithms        | Rare     | FIFO validation counts cold loads as hits (off-by-one)|
