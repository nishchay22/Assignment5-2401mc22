#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_PROCS 3

// CPU-bound workload function
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
  int order = 1;

  printf(1, "=== Question 1: FCFS Scheduler Test ===\n");
  printf(1, "Forking %d processes at staggered times...\n\n", NUM_PROCS);

  for(i = 0; i < NUM_PROCS; i++){
    // Stagger process creation times using a delay loop before each fork()
    // so that each child gets a strictly increasing ctime (in ticks).
    cpu_work(300);

    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }

    if(pid == 0){
      // Child process: run heavy CPU-bound loop
      int mypid = getpid();
      cpu_work(1000);
      printf(1, "[Child] Process %d (PID %d) finished CPU-bound work\n", i + 1, mypid);
      exit();
    } else {
      printf(1, "[Parent] Created Process %d with PID %d at tick %d\n", i + 1, pid, uptime());
    }
  }

  printf(1, "\n--- Completion Order ---\n");
  for(i = 0; i < NUM_PROCS; i++){
    int done_pid = wait();
    printf(1, "Completion #%d -> PID %d\n", order++, done_pid);
  }

  printf(1, "=== FCFS Test Complete ===\n");
  exit();
}
