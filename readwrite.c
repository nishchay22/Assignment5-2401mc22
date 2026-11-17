// Question 3: Readers-Writers Problem in xv6
// Implements the First Readers-Writers problem (readers can read concurrently)
// enhanced with a fairness turnstile semaphore so waiting writers are never
// starved indefinitely by a continuous stream of arriving readers.

#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_READERS 3
#define NUM_WRITERS 2
#define READ_ROUNDS 3
#define WRITE_ROUNDS 2

// Kernel semaphore IDs
#define SEM_MUTEX      4  // Protects read_count
#define SEM_RW_MUTEX   5  // Exclusive writer lock / first-last reader lock on shared_data
#define SEM_TURNSTILE  6  // Starvation-prevention gate (blocks new reader waves when a writer waits)
#define SEM_PRINT      7  // Protects console output so log lines never interleave

struct rw_shm {
  volatile int shared_data; // Shared integer variable read/updated
  volatile int read_count;  // Number of currently active readers
};

void
reader_proc(int id, struct rw_shm *shm)
{
  int r;
  int pid = getpid();

  for(r = 1; r <= READ_ROUNDS; r++){
    // Pass through turnstile so a waiting writer is not starved indefinitely,
    // then immediately release turnstile so multiple readers enter concurrently!
    sem_wait(SEM_TURNSTILE);
    sem_post(SEM_TURNSTILE);

    // First reader locks SEM_RW_MUTEX to exclude writers
    sem_wait(SEM_MUTEX);
    shm->read_count++;
    if(shm->read_count == 1){
      sem_wait(SEM_RW_MUTEX);
    }
    int active = shm->read_count;
    sem_post(SEM_MUTEX);

    // --- Concurrent Reading Section ---
    sem_wait(SEM_PRINT);
    printf(1, "[Tick %d] Reader %d (PID %d) ENTERED read CS -> shared_data = %d (active_readers = %d)\n",
           uptime(), id, pid, shm->shared_data, active);
    sem_post(SEM_PRINT);

    // Hold read section for 4 ticks so concurrent readers overlap simultaneously
    sleep(4);

    // Last reader unlocks SEM_RW_MUTEX to allow waiting writers in
    sem_wait(SEM_MUTEX);
    shm->read_count--;
    int rem = shm->read_count;
    sem_wait(SEM_PRINT);
    printf(1, "[Tick %d] Reader %d (PID %d) LEAVING read CS (remaining active_readers = %d)\n",
           uptime(), id, pid, rem);
    sem_post(SEM_PRINT);
    if(shm->read_count == 0){
      sem_post(SEM_RW_MUTEX);
    }
    sem_post(SEM_MUTEX);

    sleep(3);
  }
  exit();
}

void
writer_proc(int id, struct rw_shm *shm)
{
  int w;
  int pid = getpid();

  // Slight initial offset so first wave of 3 readers demonstrates concurrent reading first
  sleep(2);

  for(w = 1; w <= WRITE_ROUNDS; w++){
    // Acquire turnstile to block new incoming readers from starving this writer,
    // then wait for currently active readers/writers to release SEM_RW_MUTEX.
    sem_wait(SEM_TURNSTILE);
    sem_wait(SEM_RW_MUTEX);

    // --- Exclusive Writing Section ---
    shm->shared_data += 10;
    sem_wait(SEM_PRINT);
    printf(1, "[Tick %d] *** Writer %d (PID %d) EXCLUSIVE WRITE -> incremented shared_data = %d (active_readers = %d) ***\n",
           uptime(), id, pid, shm->shared_data, shm->read_count);
    sem_post(SEM_PRINT);

    sleep(2);

    sem_wait(SEM_PRINT);
    printf(1, "[Tick %d] *** Writer %d (PID %d) LEAVING write CS ***\n",
           uptime(), id, pid);
    sem_post(SEM_PRINT);

    sem_post(SEM_RW_MUTEX);
    sem_post(SEM_TURNSTILE);

    sleep(5);
  }
  exit();
}

int
main(int argc, char *argv[])
{
  int i;

  printf(1, "=== Question 3: Readers-Writers Problem (%d Readers, %d Writers) ===\n",
         NUM_READERS, NUM_WRITERS);

  int addr = shm_get();
  if(addr < 0){
    printf(1, "shm_get failed\n");
    exit();
  }
  struct rw_shm *shm = (struct rw_shm*)addr;
  shm->shared_data = 100;
  shm->read_count = 0;

  sem_init(SEM_MUTEX,     1);
  sem_init(SEM_RW_MUTEX,  1);
  sem_init(SEM_TURNSTILE, 1);
  sem_init(SEM_PRINT,     1);

  // Spawn 3 Reader processes
  for(i = 1; i <= NUM_READERS; i++){
    if(fork() == 0){
      reader_proc(i, shm);
    }
  }

  // Spawn 2 Writer processes
  for(i = 1; i <= NUM_WRITERS; i++){
    if(fork() == 0){
      writer_proc(i, shm);
    }
  }

  // Wait for all 5 children (3 readers + 2 writers)
  for(i = 0; i < NUM_READERS + NUM_WRITERS; i++){
    wait();
  }

  printf(1, "\n--- Verification ---\n");
  printf(1, "Final shared_data = %d (Expected: %d after %d writer increments of +10)\n",
         shm->shared_data, 100 + (NUM_WRITERS * WRITE_ROUNDS * 10), NUM_WRITERS * WRITE_ROUNDS);
  printf(1, "=== Readers-Writers Test Complete ===\n");
  exit();
}
