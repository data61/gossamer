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

#ifndef STD_MUTEX
#include <mutex>
#define STD_MUTEX
#endif

#ifndef STD_DEQUE
#include <deque>
#define STD_DEQUE
#endif

#ifndef BOOST_NONCOPYABLE_HPP
#include <boost/noncopyable.hpp>
#define BOOST_NONCOPYABLE_HPP
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
    std::deque<std::thread*> mThreads;
    mutable std::mutex mMut;
};


#endif // THREADGROUP_HH
