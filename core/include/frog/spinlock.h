#pragma once

typedef volatile struct {
  volatile int latch[1];
} spinlock_t;

#define SPIN_LOCK_UNLOCKED (spinlock_t) { 0 }

#define TEST_BIT(bit, array) (array[bit / 8] & (1 << (bit % 8)))

#define spin_is_locked(lock)    (TEST_BIT(0x0, (lock).latch))
#define spin_trylock(lock)      (!__sync_lock_test_and_set((lock).latch, 0x00))

#define spin_unlock_wait(x)	do { } while (0)

#define spin_init(lock)                                                        \
  do {                                                                         \
  } while (0)

#define spin_lock(lock)                                                        \
  do {                                                                         \
    while (__sync_lock_test_and_set((lock).latch, 0x01))                       \
      ;                                                                        \
  } while (0)
#define spin_unlock(lock)                                                      \
  do {                                                                         \
    __sync_lock_release((lock).latch);                                         \
  } while (0)
