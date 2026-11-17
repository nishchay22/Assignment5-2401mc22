// Question 4: Dining Philosophers Problem (Deadlock Avoidance) in xv6
// 5 Philosophers and 5 Forks (represented as binary semaphores).
// Deadlock Avoidance Strategy: Resource Ordering (Dijkstra's Hierarchy Solution)
// - Every philosopher picks up the lower-numbered fork first, then the higher-numbered fork:
//     first_fork  = min(left, right)
//     second_fork = max(left, right)
// - This breaks Coffman's Circular Wait condition, making deadlock mathematically impossible.

#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_PHILOSOPHERS 5
#define DEFAULT_CYCLES   5

// Kernel semaphore IDs: forks 0..4 use semaphores 10..14, print lock uses 15
#define FORK_SEM_BASE 10
#define SEM_PRINT     15

struct dining_shm {
  volatile int meals_eaten[NUM_PHILOSOPHERS];
  volatile int total_meals;
};

void
philosopher(int id, int cycles, struct dining_shm *shm)
{
  int left = id;
  int right = (id + 1) % NUM_PHILOSOPHERS;

  // Resource Ordering: pick lower-numbered fork first, higher-numbered fork second
  int first_fork  = (left < right) ? left : right;
  int second_fork = (left < right) ? right : left;

  int c;
  for(c = 1; c <= cycles; c++){
    // 1. THINKING state
    sem_wait(SEM_PRINT);
    printf(1, "Philosopher %d: THINKING (cycle %d/%d)\n", id, c, cycles);
    sem_post(SEM_PRINT);
    sleep(2);

    // 2. HUNGRY state
    sem_wait(SEM_PRINT);
    printf(1, "Philosopher %d: THINKING -> HUNGRY (wants forks %d and %d in order %d -> %d)\n",
           id, left, right, first_fork, second_fork);
    sem_post(SEM_PRINT);

    // Acquire forks in strict ascending resource order
    sem_wait(FORK_SEM_BASE + first_fork);
    sem_wait(FORK_SEM_BASE + second_fork);

    // 3. EATING state
    shm->meals_eaten[id]++;
    shm->total_meals++;
    sem_wait(SEM_PRINT);
    printf(1, "Philosopher %d: HUNGRY -> EATING -> THINKING (ate meal %d/%d using forks %d & %d)\n",
           id, c, cycles, first_fork, second_fork);
    sem_post(SEM_PRINT);

    sleep(2);

    // Release both forks
    sem_post(FORK_SEM_BASE + second_fork);
    sem_post(FORK_SEM_BASE + first_fork);
  }

  exit();
}

int
main(int argc, char *argv[])
{
  int cycles = DEFAULT_CYCLES;
  if(argc > 1){
    int n = atoi(argv[1]);
    if(n > 0)
      cycles = n;
  }

  printf(1, "=== Question 4: Dining Philosophers (%d Philosophers, %d Cycles Each) ===\n",
         NUM_PHILOSOPHERS, cycles);
  printf(1, "Deadlock Avoidance Strategy: Resource Ordering (pick lower-numbered fork first)\n\n");

  int addr = shm_get();
  if(addr < 0){
    printf(1, "shm_get failed\n");
    exit();
  }
  struct dining_shm *shm = (struct dining_shm*)addr;

  int i;
  for(i = 0; i < NUM_PHILOSOPHERS; i++){
    shm->meals_eaten[i] = 0;
    sem_init(FORK_SEM_BASE + i, 1); // Each fork is a binary semaphore initialized to 1
  }
  shm->total_meals = 0;
  sem_init(SEM_PRINT, 1);

  // Spawn 5 philosopher processes
  for(i = 0; i < NUM_PHILOSOPHERS; i++){
    if(fork() == 0){
      philosopher(i, cycles, shm);
    }
  }

  // Wait for all 5 philosophers to complete all N cycles
  for(i = 0; i < NUM_PHILOSOPHERS; i++){
    wait();
  }

  printf(1, "\n--- Verification (Deadlock-Free & Starvation-Free) ---\n");
  for(i = 0; i < NUM_PHILOSOPHERS; i++){
    printf(1, "Philosopher %d completed %d/%d meals\n", i, shm->meals_eaten[i], cycles);
  }
  printf(1, "Total meals completed: %d/%d -> ZERO DEADLOCK, ALL PHILOSOPHERS FINISHED!\n",
         shm->total_meals, NUM_PHILOSOPHERS * cycles);
  printf(1, "=== Dining Philosophers Test Complete ===\n");
  exit();
}
