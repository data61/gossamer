// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SPINLOCK_HH
#define SPINLOCK_HH

// Spinlocks are hard to port.  Here's the general strategy:
//     - On Mac OSX, the OS provides them.
//     - On Linux, use GCC test and set where possible.
//     - On Windows, the OS only supports spinlocks in the DDK.
//       So use Interlocked*Exchange.
//
// On Intel ISA (i.e. everything so far), we try to use the pause
// instruction in spin loops. This gives a hint to the CPU that it
// shouldn't saturate the bus with cache acquire requests until some
// other CPU issues a write.
//
// Pause is technically only supported in SSE2 or higher. This is
// supported on every x86_64 CPU released to date, but in theory may
// not be guaranteed. Thankfully, it's backwards compatible with IA32,
// since it's the same as rep ; nop.


// OS X spinlocks changed in 10.12 Sierra.
#if defined(GOSS_MACOSX_X64) && defined(OSSPINLOCK_DEPRECATED)
#  include <os/lock.h>
#endif



class Spinlock
{
private:

#if defined(GOSS_MACOSX_X64)
#  if defined(OSSPINLOCK_DEPRECATED)
    os_unfair_lock mLock;
#  else
    OSSpinLock mLock;
#  endif
#elif defined(GOSS_WINDOWS_X64)
    int64_t mLock;
#else
    volatile uint64_t mLock;
#endif

public:
    void lock()
    {
#if defined(GOSS_MACOSX_X64)
#  if defined(OSSPINLOCK_DEPRECATED)
        os_unfair_lock_lock(&mLock);
#  else
        OSSpinLockLock(&mLock);
#  endif
#elif defined(GOSS_WINDOWS_X64)
        while (_InterlockedCompareExchange64(&mLock, 1, 0) != 0)
        {
            YieldProcessor;
        }
#else
        while (__sync_lock_test_and_set(&mLock, 1))
        {
            do
            {
                _mm_pause();
            } while (mLock);
        }
#endif
    }

    void unlock()
    {
#if defined(GOSS_MACOSX_X64)
#  if defined(OSSPINLOCK_DEPRECATED)
        os_unfair_lock_unlock(&mLock);
#  else
        OSSpinLockUnlock(&mLock);
#  endif
#elif defined(GOSS_WINDOWS_X64)
        _InterlockedExchange64(&mLock, 0);
#else
        __sync_lock_release(&mLock);
#endif
    }

    Spinlock()
    {
#ifdef GOSS_MACOSX_X64
#  if defined(OSSPINLOCK_DEPRECATED)
        mLock = OS_UNFAIR_LOCK_INIT;
#  else
        mLock = OS_SPINLOCK_INIT;
#  endif
#else
        mLock = 0;
#endif
    }
};

class SpinlockHolder
{
public:
    SpinlockHolder(Spinlock& pLock)
        : mLock(pLock)
    {
        mLock.lock();
    }

    ~SpinlockHolder()
    {
        mLock.unlock();
    }

private:
    Spinlock& mLock;
};

#endif // SPINLOCK_HH
