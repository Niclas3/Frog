#pragma once
typedef volatile struct {
  volatile int latch[1];
} spin_lock_t;

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
