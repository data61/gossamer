#ifndef ATOMIC_HH
#define ATOMIC_HH

class Atomic
{
public:

    uint64_t get() const
    {
        return mItem;
    }

    Atomic& operator++()
    {
#if defined(GOSS_LINUX_X64)
        __sync_add_and_fetch(&mItem, 1);
#elif defined(GOSS_WINDOWS_X64)
        (uint64_t)_InterlockedIncrement64((int64_t*)&mItem);
#elif defined(GOSS_MACOSX_X64)
        (uint64_t)OSAtomicIncrement64Barrier((int64_t*)&mItem);
#else
#error "Not ported to this platform"
#endif
        return *this;
    }

    Atomic& operator+=(uint64_t pVal)
    {
#if defined(GOSS_LINUX_X64)
        __sync_add_and_fetch(&mItem, pVal);
#elif defined(GOSS_WINDOWS_X64)
        (uint64_t)_InterlockedAdd64((int64_t*)&mItem, (int64_t)pVal);
#elif defined(GOSS_MACOSX_X64)
        (uint64_t)OSAtomicAdd64Barrier((int64_t)pVal, (int64_t*)&mItem);
#else
#error "Not ported to this platform"
#endif
        return *this;
    }

    void clear()
    {
#if defined(GOSS_LINUX_X64)
        mItem = 0;
        __sync_synchronize();
#elif defined(GOSS_WINDOWS_X64)
        _InterlockedExchange64(&mItem, 0);
#elif defined(GOSS_MACOSX_X64)
        mItem = 0;
        OSMemoryBarrier();
#else
#error "Not ported to this platform"
#endif
    }

    Atomic(uint64_t pItem = 0)
        : mItem(pItem)
    {
    }

private:

#if defined(GOSS_WINDOWS_X64)
    volatile __int64 mItem;
#else
    uint64_t mItem;
#endif

};

#endif // ATOMIC_HH
