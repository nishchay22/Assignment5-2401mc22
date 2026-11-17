# Comparative Analysis of CPU Schedulers in xv6 (Lab Assignment 3)

## 1. Overview of Implemented Schedulers

| Scheduler | Type | Selection Criterion | Starvation Prevention | Context-Switch Overhead |
| :--- | :--- | :--- | :--- | :--- |
| **Default xv6 (Round-Robin)** | Preemptive (1-tick quantum) | Next `RUNNABLE` process in circular `ptable.proc` order | Inherent (fair time-slicing) | High (switches every timer tick) |
| **Q1: FCFS** | Non-Preemptive | Smallest `ctime` (creation time in `ticks`) | Inherent (arrival order is strictly finite) | Minimal (runs until `exit()` or `sleep()`) |
| **Q2: Priority + Aging** | Preemptive | Lowest `priority` number (`1` = highest, `20` = lowest), ties broken Round-Robin | **Aging**: `wait_ticks > 30` $\implies$ `priority--` | Moderate to High (preempts on timer tick; switches on ties or aging) |
| **Q3: SJF (Burst Estimate)** | Non-Preemptive | Smallest positive `burst_time` (`<= 0` scheduled last) | None without aging (continuous short jobs can delay long jobs) | Minimal (runs to completion) |

---

## 2. Turnaround Time Analysis: CPU-Bound Workloads

For a set of $n$ processes arriving at approximately the same time ($t_0$) with CPU burst times $B_1, B_2, \dots, B_n$, the **Turnaround Time (TAT)** of process $i$ is:

$$\text{TAT}_i = T_{\text{completion}, i} - T_{\text{arrival}, i}$$

### Winner for CPU-Bound Workloads: **Shortest Job First (SJF)**
* **Mathematical Optimality**: When processes execute purely CPU-bound loops (`fcfstest`, `priotest`, `sjftest`), **SJF** produces the **minimum possible average turnaround time** and **minimum average waiting time**. By executing shorter bursts first ($B_{(1)} \le B_{(2)} \le \dots \le B_{(n)}$), a short job of length $B_{(1)}$ immediately finishes and only adds $B_{(1)}$ waiting time to the remaining $n-1$ jobs, whereas running a long job $B_{(n)}$ first adds $B_{(n)}$ waiting time to all shorter jobs.
* **Observed Verification (`sjftest`)**: When 4 processes were forked with burst estimates `[80, 20, 60, 10]`, SJF reordered execution to `10 -> 20 -> 60 -> 80`. Under FCFS, the `burst = 80` process would have blocked the `burst = 10` and `burst = 20` processes—the classic **Convoy Effect**.
* **Non-Preemptive Advantage**: Both SJF and FCFS avoid per-tick context switches (`swtch()` and `switchuvm()` TLB flushes) during CPU-bound execution, completing pure computation slightly faster in wall-clock ticks than preemptive schedulers.

---

## 3. Turnaround Time & Responsiveness: Mixed (CPU + I/O) Workloads

In a realistic mixed workload (e.g., interactive shell `sh` or I/O-bound processes mixed with long CPU-bound background workers):

### Winner for Mixed Workloads: **Preemptive Priority Scheduler with Aging**
* **Why Non-Preemptive Schedulers (FCFS & SJF) Struggle on Mixed Workloads**:
  * In xv6, once a non-preemptive scheduler (`SCHED_FCFS` or `SCHED_SJF`) dispatches a long CPU-bound process, timer interrupts (`T_IRQ0 + IRQ_TIMER`) do not preempt it.
  * Even if an I/O-bound process wakes up from `sleep()` (becoming `RUNNABLE` via `wakeup1()`), it **cannot regain the CPU** until the currently running CPU-bound process voluntarily sleeps or calls `exit()`. This causes poor response time and under-utilizes I/O devices while I/O-bound processes sit stuck behind a running CPU hog.
* **Why Preemptive Priority + Aging Excels on Mixed Workloads**:
  1. **Immediate Interactive/I/O Responsiveness**: Interactive and I/O-bound tasks can be assigned higher priorities (lower priority numbers, e.g., `1–5`). Whenever an I/O interrupt wakes a high-priority process, the very next timer tick (`yield()`) preempts the background CPU-bound worker and switches to the high-priority task.
  2. **Round-Robin Tie-Breaking**: Equal-priority processes share the CPU fairly on each timer tick rather than monopolizing the core.
  3. **Guaranteed Progress via Aging**: Without aging, a continuous stream of high-priority processes would starve a priority-`16` background task indefinitely. By incrementing `wait_ticks` on every waiting tick and decrementing `priority` by `1` whenever `wait_ticks > 30`, even the lowest-priority process steadily climbs in priority and is guaranteed bounded waiting time.
