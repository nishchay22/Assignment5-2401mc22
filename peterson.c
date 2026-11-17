// Question 1: Mutual Exclusion using Peterson's Algorithm in xv6
// Uses a shared memory page (shm_get) shared between parent (Process 0)
// and child (Process 1) after fork(). Mutual exclusion is enforced
// purely via Peterson's flag[2] and turn variables in shared memory.

#include "types.h"
#include "stat.h"
#include "user.h"

#define ITERATIONS 10

// Shared memory layout for Peterson's Algorithm
struct peterson_shm {
  volatile int flag[2];        // Intent of each process (0 or 1) to enter CS
  volatile int turn;           // Whose turn it is when both want to enter
  volatile int shared_counter; // Shared counter protected by Peterson's Algorithm
};

// Short CPU delay loop for the remainder section
void
delay_loop(int loops)
{
  volatile int i;
  for(i = 0; i < loops; i++){
    asm volatile("");
  }
}

void
run_peterson(struct peterson_shm *shm, int id)
{
  int other = 1 - id;
  int iter;

  for(iter = 0; iter < ITERATIONS; iter++){
    // --- Entry Section (Peterson's Algorithm) ---
    shm->flag[id] = 1;
    shm->turn = other;
    __sync_synchronize(); // Full memory barrier to prevent load/store reordering

    while(shm->flag[other] == 1 && shm->turn == other){
      // Busy-wait while the other process wants in and it is their turn
    }

    // --- Critical Section ---
    int val = shm->shared_counter + 1;
    delay_loop(50000); // Small delay inside CS to test for race conditions
    shm->shared_counter = val;
    printf(1, "Process %d in CS, counter = %d (iteration %d/%d)\n",
           id, shm->shared_counter, iter + 1, ITERATIONS);
    printf(1, "Process %d exiting CS\n", id);

    // --- Exit Section ---
    __sync_synchronize();
    shm->flag[id] = 0;

    // --- Remainder Section ---
    sleep(1);
  }
}

int
main(int argc, char *argv[])
{
  struct peterson_shm *shm;
  int pid;

  printf(1, "=== Question 1: Mutual Exclusion using Peterson's Algorithm ===\n");

  // Set up shared memory page before fork()
  int shm_addr = shm_get();
  if(shm_addr < 0){
    printf(1, "shm_get failed\n");
    exit();
  }
  shm = (struct peterson_shm*)shm_addr;

  // Initialize Peterson's variables and shared counter
  shm->flag[0] = 0;
  shm->flag[1] = 0;
  shm->turn = 0;
  shm->shared_counter = 0;

  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }

  if(pid == 0){
    // Child is Process 1
    run_peterson(shm, 1);
    exit();
  } else {
    // Parent is Process 0
    run_peterson(shm, 0);
    wait();
  }

  printf(1, "\n--- Verification ---\n");
  printf(1, "Expected final shared_counter = %d\n", ITERATIONS * 2);
  printf(1, "Actual final shared_counter   = %d\n", shm->shared_counter);
  if(shm->shared_counter == ITERATIONS * 2)
    printf(1, "RESULT: SUCCESS (Mutual exclusion verified, zero lost updates!)\n");
  else
    printf(1, "RESULT: FAILED (Race condition detected!)\n");

  exit();
}
