// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef DEBUGALLOCATOR_HH
#define DEBUGALLOCATOR_HH

#include <mcheck.h>

template <class T>
class DebugAllocator
{
public:
    static std::set<const uint64_t*>* mLiveBarriers;

public:
    typedef T                 value_type;
    typedef value_type*       pointer;
    typedef const value_type* const_pointer;
    typedef value_type&       reference;
    typedef const value_type& const_reference;
    typedef std::size_t       size_type;
    typedef std::ptrdiff_t    difference_type;

    template <class U> 
    struct rebind { typedef DebugAllocator<U> other; };

    DebugAllocator()
    {
        //std::cerr << "item size = " << sizeof(T) << std::endl;
    }

    DebugAllocator(const DebugAllocator&) {}

    template <class U> 
    DebugAllocator(const DebugAllocator<U>&) {}

    ~DebugAllocator() {}

    uint64_t* pre(pointer p) const
    {
        return &(reinterpret_cast<uint64_t*>(p)[-1]);
    }

    uint64_t* post(pointer p, size_type n) const
    {
        return reinterpret_cast<uint64_t*>(&(p[n]));
    }

    pointer address(reference x) const
    {
        return &x;
    }

    const_pointer address(const_reference x) const
    { 
        return x;
    }

    pointer allocate(size_type n, const_pointer = 0)
    {
        if (!mLiveBarriers)
        {
            mLiveBarriers = new std::set<const uint64_t*>();
        }

        void* p = std::malloc(n * sizeof(T) + 2 * sizeof(uint64_t));
        if (!p)
        {
            throw std::bad_alloc();
        }
        pointer item = reinterpret_cast<T*>(reinterpret_cast<uint64_t*>(p) + 1);
        *pre(item) = 0xDEADBEEFDEADBEEFULL;
        std::cerr << '+' << pre(item) << '\t' << mLiveBarriers << std::endl;
        BOOST_ASSERT(mLiveBarriers->count(pre(item)) == 0);
        mLiveBarriers->insert(pre(item));
        *post(item, n) = 0xDEADBEEFDEADBEEFULL;
        std::cerr << '+' << post(item, n) << '\t' << mLiveBarriers << std::endl;
        BOOST_ASSERT(mLiveBarriers->count(post(item, n)) == 0);
        mLiveBarriers->insert(post(item, n));
        return item;
    }

    void deallocate(pointer item, size_type n)
    {
        void* p = reinterpret_cast<uint64_t*>(item) - 1;
        if (*pre(item) != 0xDEADBEEFDEADBEEFULL)
        {
            std::cerr << "watch " << pre(item) << std::endl;
            throw item;
        }
        if (*post(item, n) != 0xDEADBEEFDEADBEEFULL)
        {
            std::cerr << "watch " << post(item, n) << std::endl;
            throw item;
        }
        std::cerr << '-' << pre(item) << '\t' << mLiveBarriers << std::endl;
        std::cerr << '-' << post(item, n) << '\t' << mLiveBarriers << std::endl;
        BOOST_ASSERT(mLiveBarriers->count(pre(item)));
        mLiveBarriers->erase(pre(item));
        BOOST_ASSERT(mLiveBarriers->count(post(item, n)));
        mLiveBarriers->erase(post(item, n));
        std::free(p);
    }

    size_type max_size() const
    { 
        return static_cast<size_type>(-1) / sizeof(T);
    }

    void construct(pointer p, const value_type& x)
    { 
        new(p) value_type(x); 
    }

    void destroy(pointer p)
    {
        p->~value_type();
    }

private:
    void operator=(const DebugAllocator&);
};

template<>
class DebugAllocator<void>
{
    typedef void        value_type;
    typedef void*       pointer;
    typedef const void* const_pointer;

    template <class U> 
        struct rebind { typedef DebugAllocator<U> other; };
};


template <class T>
inline bool operator==(const DebugAllocator<T>&, 
        const DebugAllocator<T>&) {
    return true;
}

template <class T>
inline bool operator!=(const DebugAllocator<T>&, 
        const DebugAllocator<T>&) {
    return false;
}



#endif // DEBUGALLOCATOR_HH
