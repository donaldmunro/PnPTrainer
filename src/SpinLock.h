#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <atomic>

class SpinLock
//============
{
public:
   SpinLock() { atomic_lock.clear(); }

   SpinLock(const SpinLock &) = delete;

   ~SpinLock() = default;

   void lock() { while (atomic_lock.test_and_set(std::memory_order_acquire)); }

   bool try_lock() { return !atomic_lock.test_and_set(std::memory_order_acquire); }

   void unlock() { atomic_lock.clear(std::memory_order_release); }

private:
   std::atomic_flag atomic_lock;
};

#endif
