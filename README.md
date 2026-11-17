# Operating Systems Lab Assignment 5: Process Synchronization in xv6

## 1. Kernel Synchronization & Shared Memory Architecture

### A. Shared Memory Page Mechanism (`shm_get` & `PTE_SHM`)
By default, xv6 gives every process an isolated page table (`pgdir`), and `fork()` invokes `copyuvm()` in `vm.c` to allocate separate physical pages for the child. To enable true shared memory between a parent and its forked children across all 4 questions:
1. **`PTE_SHM` (`0x200` in `mmu.h`)**: We use bit 9 of the x86 Page Table Entry (reserved by hardware for OS software use) to mark shared memory pages.
2. **`sys_shm_get()` / `shm_get()` (`vm.c`)**: Allocates a single 4 KB physical page (`shared_phys_page`), zero-initializes it, maps it at `va = PGROUNDUP(curproc->sz)` with permissions `PTE_W | PTE_U | PTE_SHM`, advances `curproc->sz`, and returns the virtual address pointer to user space.
3. **Inheritance Across `fork()` (`copyuvm` in `vm.c`)**: When `copyuvm()` encounters a page with `PTE_SHM` set, instead of calling `kalloc()` and copying memory, it maps the **exact same physical address `pa`** into the child's page directory at the same virtual address.
4. **Safe Deallocation (`deallocuvm` in `vm.c`)**: When a child process exits, `deallocuvm()` checks `(*pte & PTE_SHM) == 0` before calling `kfree()`, preventing premature deallocation or double-free panics while sibling processes or the parent are still using the shared page.

### B. Lightweight Kernel Semaphores (`sem_init`, `sem_wait`, `sem_post`)
In `proc.c`, we implement a table of `NSEM = 32` kernel semaphores (`struct ksem semtable[NSEM]`), each containing an xv6 `struct spinlock` and an integer `value`:
* **`sem_init(int sem_id, int value)`**: Acquires `semtable[sem_id].lock` and initializes the counter to `value` (used as a counting semaphore when `value > 1`, or a binary mutex/lock when `value == 1`).
* **`sem_wait(int sem_id)` ($P$ / `down`)**: Acquires the semaphore's spinlock and loops `while (semtable[sem_id].value <= 0) sleep(&semtable[sem_id], &semtable[sem_id].lock);`. Using xv6's `sleep()` atomically releases the spinlock and puts the calling process into the `SLEEPING` state (avoiding CPU-wasting spin waits), and decrements `value--` upon waking.
* **`sem_post(int sem_id)` ($V$ / `up`)**: Acquires the semaphore's spinlock, increments `value++`, and calls `wakeup(&semtable[sem_id])` to transition waiting processes to `RUNNABLE`.

---

## 2. Question-by-Question Design & Write-Up

### Question 1: Mutual Exclusion using Peterson's Algorithm (`peterson.c`)
* **Design**: Before calling `fork()`, the parent maps `struct peterson_shm` via `shm_get()`, containing `volatile int flag[2]`, `volatile int turn`, and `volatile int shared_counter`.
* **Mutual Exclusion Logic**: Process `0` (parent) and Process `1` (child) each execute 10 iterations using pure software synchronization:
  ```c
  shm->flag[id] = 1;
  shm->turn = other;
  __sync_synchronize(); // Full hardware memory barrier
  while(shm->flag[other] == 1 && shm->turn == other) ;
  ```
  Hardware memory barriers (`__sync_synchronize()`) prevent the compiler or x86 store buffer from reordering `flag[id] = 1` and `turn = other` before reading `flag[other]`. Both `shared_counter` updates and `printf` calls execute strictly inside the critical section, reaching `shared_counter = 20` with zero lost updates and zero interleaved critical sections.

---

### Question 2: Producer-Consumer Problem (`prodcons.c`)
* **Shared State & Primitives**: A 5-slot circular buffer (`struct shared_buffer` with `buffer[]`, `in`, `out`, `count`, `buf_size`) is placed in shared memory (`shm_get()`). Synchronization uses:
  * `SEM_EMPTY` (counting semaphore initialized to `buf_size = 5`),
  * `SEM_FULL` (counting semaphore initialized to `0`),
  * `SEM_MUTEX` (binary semaphore initialized to `1`).
* **Blocking & Unblocking Verification**:
  * At startup and during items 11–20 (when the Consumer runs faster than the Producer), `count` drops to `0` and the Consumer blocks inside `sem_wait(SEM_FULL)` until the Producer inserts an item and calls `sem_post(SEM_FULL)`.
  * During items 1–10 (when the Producer runs faster than the Consumer), `count` reaches `5/5` and the Producer blocks inside `sem_wait(SEM_EMPTY)` until the Consumer removes an item and calls `sem_post(SEM_EMPTY)`.
  * All 20 items (`1` through `20`) are consumed in exact FIFO order without loss or duplication.

---

### Question 3: Readers-Writers Problem (`readwrite.c`)
* **Readers-Priority vs. Starvation-Free Solution**:
  * **Pure First Readers-Writers (Readers-Priority)**: The first arriving reader (`read_count == 1`) acquires `SEM_RW_MUTEX` (blocking writers), and subsequent readers (`read_count > 1`) enter concurrently without waiting on `SEM_RW_MUTEX`. Only the last exiting reader (`read_count == 0`) releases `SEM_RW_MUTEX`. **Flaw**: If new readers keep arriving while at least one reader is active (`read_count > 0`), `read_count` never hits `0` and a waiting writer starves indefinitely.
  * **Our Starvation-Free Solution (`SEM_TURNSTILE`)**: We add a fairness gate semaphore `SEM_TURNSTILE` (initialized to `1`):
    * **Readers** execute `sem_wait(SEM_TURNSTILE); sem_post(SEM_TURNSTILE);` before incrementing `read_count`. When no writer is waiting, readers pass through `SEM_TURNSTILE` immediately and read `shared_data` **simultaneously** (`active_readers = 1 -> 2 -> 3`).
    * **Writers** execute `sem_wait(SEM_TURNSTILE); sem_wait(SEM_RW_MUTEX);`. As soon as a writer arrives and waits on `SEM_RW_MUTEX`, it holds `SEM_TURNSTILE`, preventing *new* waves of readers from jumping ahead while allowing already-active readers to finish and drain (`read_count -> 0`). Once the writer finishes its exclusive update (`active_readers == 0`), it releases both `SEM_RW_MUTEX` and `SEM_TURNSTILE`.

---

### Question 4: Dining Philosophers Problem — Deadlock Avoidance (`dining.c`)
* **Deadlock-Avoidance Strategy Used: Resource Ordering (Dijkstra's Hierarchy Solution)**:
  * Each of the 5 forks (`0..4`) is represented as a binary semaphore (`sem_init(FORK_SEM_BASE + i, 1)`).
  * For Philosopher $i \in \{0, 1, 2, 3, 4\}$, the adjacent forks are $\text{left} = i$ and $\text{right} = (i + 1) \bmod 5$.
  * Instead of every philosopher picking up their left fork first (which causes a circular wait $F_0 \to F_1 \to F_2 \to F_3 \to F_4 \to F_0$ when all 5 become `HUNGRY` simultaneously), every philosopher acquires the **lower-numbered fork first** and the **higher-numbered fork second**:
    $$\text{first\_fork} = \min(\text{left}, \text{right}), \qquad \text{second\_fork} = \max(\text{left}, \text{right})$$
* **Why Resource Ordering Prevents Deadlock**:
  * Specifically, Philosophers `0, 1, 2, 3` pick up `left` ($i$) then `right` ($i+1$), whereas **Philosopher `4`** (whose forks are `left = 4` and `right = 0`) picks up **`right` (`0`) first, then `left` (`4`)**.
  * By Coffman's deadlock theorem, deadlock requires four simultaneous conditions: *Mutual Exclusion*, *Hold and Wait*, *No Preemption*, and *Circular Wait*.
  * Because every philosopher holding fork $u$ only ever requests a second fork $v$ with $u < v$, every directed edge in the resource-wait graph strictly increases in fork index ($u < v$). Since a strictly increasing sequence of integers cannot form a cycle, **Circular Wait is impossible**, guaranteeing deadlock freedom while allowing non-adjacent philosophers to eat concurrently.

---

## 3. How to Build and Run

```bash
# Build and boot xv6 in QEMU (2 CPUs enabled by default for true concurrency)
make clean
make qemu-nox

# Inside the xv6 shell ($), run each program:
$ peterson
$ prodcons
$ readwrite
$ dining
```
*(To exit `qemu-nox`, press `Ctrl + A`, release, then press `x`.)*
