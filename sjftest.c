#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_PROCS 4

// CPU-bound workload
void
cpu_work(int loops)
{
  volatile int i, j;
  for(i = 0; i < loops; i++){
    for(j = 0; j < 100000; j++){
      asm volatile("");
    }
  }
}

int
main(int argc, char *argv[])
{
  int i, pid;
  // Intentionally out-of-order burst times to prove SJF reorders by burst_time:
  // Expected completion order: burst 10 -> burst 20 -> burst 60 -> burst 80
  int bursts[NUM_PROCS] = {80, 20, 60, 10};
  int order = 1;

  printf(1, "=== Question 3: Shortest Job First (SJF) Scheduler Test ===\n");

  for(i = 0; i < NUM_PROCS; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pid == 0){
      int mypid = getpid();
      // Declare expected burst time immediately after forking
      set_burst(mypid, bursts[i]);
      // Run CPU-bound loop proportional to burst estimate
      cpu_work(bursts[i] * 15);
      printf(1, "[Child] PID %d (declared burst_time = %d) finished\n",
             mypid, bursts[i]);
      exit();
    } else {
      // Also set burst_time from parent so it is registered before parent enters wait()
      set_burst(pid, bursts[i]);
      printf(1, "[Parent] Forked Child %d -> PID %d with declared burst_time = %d\n",
             i + 1, pid, bursts[i]);
    }
  }

  // Give parent burst_time = 1 before waiting so whenever a child exits,
  // parent immediately wakes up, reaps that child in completion order, and sleeps again.
  set_burst(getpid(), 1);

  printf(1, "\n--- Completion Order (Expected: burst 10, 20, 60, 80) ---\n");
  for(i = 0; i < NUM_PROCS; i++){
    int done_pid = wait();
    printf(1, "Completion #%d -> PID %d\n", order++, done_pid);
  }

  printf(1, "=== SJF Test Complete ===\n");
  exit();
}
