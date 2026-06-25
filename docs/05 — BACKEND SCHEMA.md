# **DOCUMENT 05 — BACKEND SCHEMA**

# **Data structures, relationships, and state model**

---

## **Core Data Structures**

### **ProcessControlBlock (PCB)**

Location: include/core/process.h

Fields: int pid — unique process identifier, set from config.yaml std::string name — human-readable name for TUI display ProcessState current\_state — enum: READY, RUNNING, BLOCKED, TERMINATED int base\_priority — immutable, set at creation from config.yaml int current\_priority — mutable, boosted by priority inheritance int burst\_time — total cycles needed, set from config.yaml int remaining\_burst\_time — decrements each cycle the process runs int arrival\_cycle — cycle at which this PCB enters the ready queue int starvation\_counter — increments each cycle spent in READY without running int flash\_countdown — render frames remaining for hit/fault colour flash std::vector\<int\> page\_references — ordered list of page numbers from config.yaml int page\_ref\_index — current position in page\_references std::vector\<int\> max\_resources — Banker's max claim \[R0, R1, R2\] std::vector\<int\> allocated\_resources — currently held resources \[R0, R1, R2\] mutable std::shared\_mutex rw\_mutex — protects all fields read by UI thread

Relationships: PCB owned by CpuScheduler via std::unique\_ptr\<ProcessControlBlock\> PCB observed (non-owning raw pointer) by ConcurrencyManager and MemoryManagementUnit

### **PCBSnapshot (read-only copy for UI thread)**

Location: include/core/process.h

Fields: int pid ProcessState state int base\_priority int current\_priority int remaining\_burst\_time int starvation\_counter bool priority\_boosted — true if current\_priority \> base\_priority

The UI thread calls PCB::get\_snapshot() which returns this struct under shared\_lock. The UI never touches the actual PCB fields directly.

### **CycleMetrics (telemetry record)**

Location: include/utils/telemetry.h

Fields: uint64\_t cycle\_id int active\_pid — PID of process that ran this cycle (-1 if idle) float cpu\_utilization\_percent int total\_page\_faults — cumulative int total\_tlb\_hits — cumulative int total\_tlb\_misses — cumulative int deadlocks\_prevented — cumulative int frames\_in\_use — at end of this cycle bool belady\_anomaly\_detected — true if FIFO faults increased with more frames bool thrashing\_detected — true if fault rate exceeded threshold this cycle

### **PageFrame (physical memory slot)**

Location: include/memory/mmu.h

Fields: int page\_number — virtual page currently occupying this frame (-1 if empty) uint64\_t load\_time — access\_counter value when this page was loaded (FIFO key) uint64\_t last\_used — access\_counter value of most recent access (LRU key) bool reference\_bit — Clock algorithm second-chance bit int flash\_countdown — render frames remaining for hit/fault flash (0 \= neutral) bool last\_event\_was\_fault — true \= flash red, false \= flash green

### **KernelMutex (synchronisation primitive wrapper)**

Location: include/core/concurrency\_manager.h

Fields: std::mutex internal\_mutex std::atomic\<int\> owner\_pid — PID of holding thread (-1 if unlocked) int owner\_base\_priority — stored at lock time for inheritance revert std::queue\<int\> wait\_queue — PIDs blocked waiting for this mutex

### **BankersMatrix (deadlock avoidance state)**

Location: include/core/concurrency\_manager.h

Fields: std::vector\<int\> available — \[R0\_avail, R1\_avail, R2\_avail\] std::vector\<std::vector\<int\>\> maximum — max\[pid\]\[resource\_type\] std::vector\<std::vector\<int\>\> allocation — allocation\[pid\]\[resource\_type\] std::vector\<std::vector\<int\>\> need — need \= maximum \- allocation

Methods: bool is\_safe\_state() const — O(n²) safety check, call only on resource request bool request\_resources(int pid, std::vector\<int\> request) void release\_resources(int pid, std::vector\<int\> release)

### **PageReplacer (abstract base class — polymorphic interface)**

Location: include/memory/page\_replacer.h

Pure virtual methods: virtual int select\_victim(const std::vector\<PageFrame\>& frames) \= 0 virtual void on\_page\_accessed(int frame\_index, uint64\_t access\_counter) \= 0 virtual void on\_page\_loaded(int frame\_index, uint64\_t access\_counter) \= 0 virtual std::string algorithm\_name() const \= 0

Concrete implementations: FifoStrategy — evicts by load\_time (oldest loaded first) LruStrategy — evicts by last\_used (least recently accessed) ClockStrategy — circular pointer \+ reference\_bit second-chance OptStrategy — look-ahead in per-process page\_references remaining

## **State Machine — ProcessState Transitions**

READY ──────────→ RUNNING : scheduler dispatches this PCB this cycle RUNNING ─────────→ READY : quantum expired (RR) or higher priority arrived (SRTF) RUNNING ─────────→ BLOCKED : process requested a locked resource (wait queue) RUNNING ─────────→ TERMINATED : remaining\_burst\_time reaches 0 BLOCKED ─────────→ READY : resource released, process moves off wait queue

## **Ownership and Lifetime Rules**

CpuScheduler owns PCBs via std::vector\<std::unique\_ptr\<ProcessControlBlock\>\> ConcurrencyManager holds non-owning raw pointers to PCBs (for priority manipulation) MemoryManagementUnit holds non-owning raw pointer to current active PCB's page\_references UI components receive PCBSnapshot copies — never raw PCB pointers TelemetryLogger owns the ofstream and the ring buffer deque

## **Thread Ownership Map**

Main thread : FTXUI screen.Loop() — only FTXUI rendering calls here Kernel thread : CpuScheduler::run\_loop() — all simulation logic here Telemetry thread : TelemetryLogger::writer\_loop() — CSV writes here

Data shared between kernel thread and main (UI) thread: → PCB fields (protected by per-PCB shared\_mutex) → PageFrame array (protected by VMM shared\_mutex) → KernelMutex wait queues (protected by ConcurrencyManager mutex) → CycleMetrics ring buffer (protected by telemetry\_mutex) → simulation\_running flag (std::atomic\<bool\>, no lock needed)

