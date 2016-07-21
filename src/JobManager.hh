#ifndef JOBMANAGER_HH
#define JOBMANAGER_HH

#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <map>
#include <set>
#include <vector>

#include "WorkQueue.hh"

class JobManager
{
public:
    typedef boost::function<void (void)> Job;

    typedef uint64_t Token;
    typedef std::set<Token> Tokens;

    Token enqueue(const Job& pJob)
    {
        Tokens deps;
        return enqueue(pJob, deps);
    }

    Token enqueue(const Job& pJob, const Token& pDep)
    {
        Tokens deps;
        deps.insert(pDep);
        return enqueue(pJob, deps);
    }

    Token enqueue(const Job& pJob, const Tokens& pDeps)
    {
        boost::unique_lock<boost::mutex> lk(mMutex);
        std::vector<Token> toSchedule;
        Token t = mNextTok++;
        mJobs[t] = pJob;
        mIncomplete.insert(t);
        mUnmetDeps[t] = pDeps;
        Tokens& deps = mUnmetDeps[t];
        std::vector<Token> dels;
        for (Tokens::iterator i = deps.begin(); i != deps.end(); ++i)
        {
            if (mIncomplete.count(*i))
            {
                mDepsIndex[*i].insert(t);
            }
            else
            {
                dels.push_back(*i);
            }
        }
        for (std::vector<Token>::const_iterator i = dels.begin(); i != dels.end(); ++i)
        {
            deps.erase(*i);
        }
        if (deps.empty())
        {
            toSchedule.push_back(t);
        }
        for (std::vector<Token>::const_iterator i = toSchedule.begin(); i != toSchedule.end(); ++i)
        {
            schedule(*i);
        }
        return t;
    }

    void wait()
    {
        boost::unique_lock<boost::mutex> lk(mMutex);
        while (!mIncomplete.empty())
        {
            mCond.wait(lk);
        }
    }

    void wait(const Token& pTok)
    {
        boost::unique_lock<boost::mutex> lk(mMutex);
        while (mIncomplete.count(pTok))
        {
            mCond.wait(lk);
        }
    }

    JobManager(uint64_t pNumThreads)
        : mNextTok(0), mQueue(pNumThreads)
    {
    }

private:

    static void runJob(JobManager* pMgr, Token pTok, Job pJob)
    {
        pJob();
        pMgr->finish(pTok);
    }

    void schedule(const Token& pTok)
    {
        Job j = mJobs[pTok];
        mJobs.erase(pTok);
        mQueue.push_back(boost::bind(&runJob, this, pTok, j));
    }

    void finish(Token pTok)
    {
        boost::unique_lock<boost::mutex> lk(mMutex);
        std::vector<Token> toSchedule;
        mIncomplete.erase(pTok);
        if (mIncomplete.empty())
        {
            mCond.notify_all();
        }
        std::map<Token,Tokens>::iterator x = mDepsIndex.find(pTok);
        if (x != mDepsIndex.end())
        {
            for (Tokens::const_iterator i = x->second.begin(); i != x->second.end(); ++i)
            {
                Tokens& deps = mUnmetDeps[*i];
                deps.erase(pTok);
                if (deps.empty())
                {
                    toSchedule.push_back(*i);
                    mUnmetDeps.erase(*i);
                }
            }
            mDepsIndex.erase(pTok);
        }
        for (std::vector<Token>::const_iterator i = toSchedule.begin(); i != toSchedule.end(); ++i)
        {
            schedule(*i);
        }
    }

    boost::mutex mMutex;
    boost::condition_variable mCond;
    Token mNextTok;
    std::map<Token,Job> mJobs;
    std::map<Token,Tokens> mDepsIndex;
    std::map<Token,Tokens> mUnmetDeps;
    Tokens mIncomplete;
    WorkQueue mQueue;
};

#endif // JOBMANAGER_HH
