#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_PROCS 4

// CPU-bound workload (~45 ticks per process)
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
  // Fork 4 processes with priorities: 16 (low), 12 (med-low), 8 (med-high), 4 (high)
  // Note: 1 = highest priority, 20 = lowest priority.
  int priorities[NUM_PROCS] = {16, 12, 8, 4};
  int order = 1;

  printf(1, "=== Question 2: Priority Preemptive Scheduler with Aging Test ===\n");

  // Give parent highest priority (1) while forking so all 4 children
  // are created and ready before competing for the CPU.
  set_priority(getpid(), 1);

  for(i = 0; i < NUM_PROCS; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pid == 0){
      int mypid = getpid();
      // Run a CPU-bound workload (~45 ticks) so waiting processes exceed
      // 30 ticks and demonstrate aging (priority number decrements by 1).
      cpu_work(900);
      printf(1, "[Child] PID %d (initial priority %d) finished at tick %d\n",
             mypid, priorities[i], uptime());
      exit();
    } else {
      set_priority(pid, priorities[i]);
      printf(1, "[Parent] Forked Child %d -> PID %d with initial priority %d\n",
             i + 1, pid, priorities[i]);
    }
  }

  printf(1, "\n--- Starting Scheduling & Aging ---\n");
  for(i = 0; i < NUM_PROCS; i++){
    int done_pid = wait();
    printf(1, "Completion #%d -> PID %d\n", order++, done_pid);
  }

  printf(1, "=== Priority + Aging Test Complete ===\n");
  exit();
}
