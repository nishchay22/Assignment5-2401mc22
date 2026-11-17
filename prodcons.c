// Question 2: Producer-Consumer Problem (Bounded Circular Buffer) in xv6
// Uses two counting semaphores (SEM_EMPTY and SEM_FULL) and one mutex
// (SEM_MUTEX) backed by xv6 kernel sleep()/wakeup(), plus a shared memory
// page (shm_get) holding the circular buffer between Producer and Consumer.

#include "types.h"
#include "stat.h"
#include "user.h"

#define DEFAULT_BUF_SIZE 5
#define MAX_BUF_SIZE     16
#define TOTAL_ITEMS      20

// Kernel semaphore IDs
#define SEM_EMPTY 0  // Counting semaphore: number of empty slots (init = buf_size)
#define SEM_FULL  1  // Counting semaphore: number of filled slots (init = 0)
#define SEM_MUTEX 2  // Binary semaphore (mutex): protects circular buffer state
#define SEM_PRINT 3  // Binary semaphore: ensures atomic console logging

struct shared_buffer {
  int buffer[MAX_BUF_SIZE];
  int in;        // Next insertion index
  int out;       // Next removal index
  int count;     // Current number of items in buffer
  int buf_size;  // Configurable capacity (default = 5)
};

int
main(int argc, char *argv[])
{
  int buf_size = DEFAULT_BUF_SIZE;
  if(argc > 1){
    int arg_sz = atoi(argv[1]);
    if(arg_sz >= 1 && arg_sz <= MAX_BUF_SIZE)
      buf_size = arg_sz;
  }

  printf(1, "=== Question 2: Producer-Consumer (Bounded Buffer, size = %d) ===\n", buf_size);

  // Allocate shared memory page for the circular buffer
  int addr = shm_get();
  if(addr < 0){
    printf(1, "shm_get failed\n");
    exit();
  }
  struct shared_buffer *shm = (struct shared_buffer*)addr;
  shm->in = 0;
  shm->out = 0;
  shm->count = 0;
  shm->buf_size = buf_size;

  // Initialize semaphores
  sem_init(SEM_EMPTY, buf_size); // Initially all buf_size slots are empty
  sem_init(SEM_FULL,  0);        // Initially 0 slots are full
  sem_init(SEM_MUTEX, 1);        // Binary mutex unlocked (1)
  sem_init(SEM_PRINT, 1);        // Print lock unlocked (1)

  // Fork Producer process
  int prod_pid = fork();
  if(prod_pid == 0){
    int item;
    // Wait briefly at start so Consumer attempts to read from empty buffer first
    sleep(3);

    for(item = 1; item <= TOTAL_ITEMS; item++){
      // Check if buffer is currently full to log blocking behavior
      sem_wait(SEM_MUTEX);
      if(shm->count == shm->buf_size){
        sem_wait(SEM_PRINT);
        printf(1, "[Producer] Buffer FULL (%d/%d) before item %d -> BLOCKING on 'empty' semaphore...\n",
               shm->count, shm->buf_size, item);
        sem_post(SEM_PRINT);
      }
      sem_post(SEM_MUTEX);

      // Wait for an empty slot and acquire buffer mutex
      sem_wait(SEM_EMPTY);
      sem_wait(SEM_MUTEX);

      // Insert item into circular buffer
      int slot = shm->in;
      shm->buffer[slot] = item;
      shm->in = (shm->in + 1) % shm->buf_size;
      shm->count++;

      sem_wait(SEM_PRINT);
      printf(1, "[Producer] Produced item %d at slot %d (buffered = %d/%d)\n",
             item, slot, shm->count, shm->buf_size);
      sem_post(SEM_PRINT);

      // Release mutex and signal that a new full slot is available
      sem_post(SEM_MUTEX);
      sem_post(SEM_FULL);

      // For items 1..10 producer runs faster than consumer (demonstrating FULL buffer block);
      // for items 11..20 producer runs slower (demonstrating EMPTY buffer block).
      if(item <= 10)
        sleep(1);
      else
        sleep(4);
    }
    exit();
  }

  // Fork Consumer process
  int cons_pid = fork();
  if(cons_pid == 0){
    int i, expected = 1, errors = 0;

    for(i = 1; i <= TOTAL_ITEMS; i++){
      // Check if buffer is currently empty to log blocking behavior
      sem_wait(SEM_MUTEX);
      if(shm->count == 0){
        sem_wait(SEM_PRINT);
        printf(1, "[Consumer] Buffer EMPTY (0/%d) before reading item #%d -> BLOCKING on 'full' semaphore...\n",
               shm->buf_size, i);
        sem_post(SEM_PRINT);
      }
      sem_post(SEM_MUTEX);

      // Wait for a filled slot and acquire buffer mutex
      sem_wait(SEM_FULL);
      sem_wait(SEM_MUTEX);

      // Remove item from circular buffer
      int slot = shm->out;
      int item = shm->buffer[slot];
      shm->out = (shm->out + 1) % shm->buf_size;
      shm->count--;

      if(item != expected)
        errors++;
      expected++;

      sem_wait(SEM_PRINT);
      printf(1, "[Consumer] Consumed item %d from slot %d (buffered = %d/%d)\n",
             item, slot, shm->count, shm->buf_size);
      sem_post(SEM_PRINT);

      // Release mutex and signal that an empty slot is now available
      sem_post(SEM_MUTEX);
      sem_post(SEM_EMPTY);

      // Sleep longer during first 10 items (so buffer fills up), shorter during items 11..20
      if(i <= 10)
        sleep(4);
      else
        sleep(1);
    }

    sem_wait(SEM_PRINT);
    printf(1, "\n--- Verification ---\n");
    printf(1, "Total items consumed: %d/%d, Out-of-order/lost errors: %d\n",
           expected - 1, TOTAL_ITEMS, errors);
    sem_post(SEM_PRINT);
    exit();
  }

  // Parent waits for both Producer and Consumer to finish
  wait();
  wait();
  printf(1, "=== Producer-Consumer Test Complete ===\n");
  exit();
}
