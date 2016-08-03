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
