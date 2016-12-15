// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef THREADGROUP_HH
#define THREADGROUP_HH

#ifndef STD_THREAD
#include <thread>
#define STD_THREAD
#endif

#ifndef STD_LIST
#include <list>
#define STD_LIST
#endif

class ThreadGroup : private boost::noncopyable
{
public:
    ThreadGroup()
    {
    }

    ~ThreadGroup()
    {
        for (auto t : mThreads) {
            delete t;
        }
    }

    template<typename F>
    std::thread*
    create(F pFunc)
    {
        std::unique_lock<std::mutex> lock(mMut);
        std::unique_ptr<std::thread> thr(new std::thread(pFunc));
        std::thread* t = thr.get();
        mThreads.push_back(t);
        thr.release();
        return t;
    }

    void
    join()
    {
        std::unique_lock<std::mutex> lock(mMut);
        for (auto t : mThreads) {
            t->join();
        }
    }

private:
    std::list<std::thread*> mThreads;
    mutable std::mutex mMut;
};


#endif // THREADGROUP_HH
