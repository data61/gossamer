// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdComputeNearKmers.hh"

#include "Utils.hh"
#include "FastaParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "KmerSet.hh"
#include "Phylogeny.hh"
#include "ProgressMonitor.hh"
#include "RunLengthCodedSet.hh"
#include "Spinlock.hh"
#include "Timer.hh"
#include "ThreadGroup.hh"

#include <string>
#include <boost/atomic.hpp>
#include <boost/lexical_cast.hpp>
#include <thread>

using namespace boost;
using namespace boost::program_options;
using namespace std;

namespace // anonymous
{
    class ProgressUpdater
    {
    public:
        void operator()()
        {
            mLog(info, lexical_cast<string>(mNumGray) + " gray kmers out of " + lexical_cast<string>(mAll));
            double d = mNumGray;
            d /= (mAll + 1);
            uint64_t p = 1000 *d;
            mLog(info, "or " + lexical_cast<string>(p / 10.0) + "%");
        }

        ProgressUpdater(Logger& pLog, uint64_t& pNumGray, uint64_t& pAll)
            : mLog(pLog), mNumGray(pNumGray), mAll(pAll)
        {
        }

    private:
        Logger& mLog;
        uint64_t& mNumGray;
        uint64_t& mAll;
    };

    class BlockWorker
    {
    public:
        void operator()()
        {
            for (uint64_t i = mBegin; i < mEnd; ++i, ++mGlobalCount)
            {
                if (!(i & mTickMask))
                {
                    unique_lock<mutex> lk(mMutex);
                    mCond.notify_one();
                }
                if (mLhs.get(i) == mRhs.get(i))
                {
                    continue;
                }
                KmerSet::Edge x = mKmerSet.select(i);
                bool found = false;
                for (uint64_t j = 0; !found && j < mKmerSet.K(); ++j)
                {
                    for (uint64_t b = 0; !found && b < 4; ++b)
                    {
                        Gossamer::position_type m(b);
                        m <<= j;
                        KmerSet::Edge y = x;
                        y.value() ^= m;
    #if 0
                        {
                            SmallBaseVector xSeq;
                            mKmerSet.seq(x, xSeq);
                            SmallBaseVector ySeq;
                            mKmerSet.seq(y, ySeq);
                            cerr << xSeq << '\t' << ySeq << endl;
                        }
    #endif
                        if (y != x)
                        {
                            mKmerSet.normalize(y);
                            uint64_t r = 0;
                            if (mKmerSet.accessAndRank(y, r))
                            {
                                if (mLhs.get(r) != mRhs.get(r) && mLhs.get(i) != mLhs.get(r))
                                {
                                    found = true;
                                }
                            }
                        }
                    }
                }
                bool lhsBit = mLhs.get(i);
                bool rhsBit = mRhs.get(i);
                if (found)
                {
                    ++mGrayCount;
                    lhsBit = rhsBit = false;
                }
                unique_lock<mutex> lk(mMutex);
                mNewLhs[i] = lhsBit;
                mNewRhs[i] = rhsBit;
            }
            unique_lock<mutex> lk(mMutex);
            mCond.notify_one();
        }

        BlockWorker(const KmerSet& pKmerSet, const WordyBitVector& pLhs, const WordyBitVector& pRhs,
                    const uint64_t& pBegin, const uint64_t& pEnd,
                    mutex& pMutex, condition_variable& pCond, dynamic_bitset<>& pNewLhs, dynamic_bitset<>& pNewRhs,
                    boost::atomic<uint64_t>& pGlobalCount, boost::atomic<uint64_t>& pGrayCount, uint64_t& pTickMask)
            : mKmerSet(pKmerSet), mLhs(pLhs), mRhs(pRhs), mBegin(pBegin), mEnd(pEnd),
              mMutex(pMutex), mCond(pCond), mNewLhs(pNewLhs), mNewRhs(pNewRhs),
              mGlobalCount(pGlobalCount), mGrayCount(pGrayCount), mTickMask(pTickMask)
        {
        }

    private:
        const KmerSet& mKmerSet;
        const WordyBitVector& mLhs;
        const WordyBitVector& mRhs;
        const uint64_t mBegin;
        const uint64_t mEnd;
        mutex& mMutex;
        condition_variable& mCond;
        dynamic_bitset<>& mNewLhs;
        dynamic_bitset<>& mNewRhs;
        boost::atomic<uint64_t>& mGlobalCount;
        boost::atomic<uint64_t>& mGrayCount;
        const uint64_t mTickMask;
    };
    typedef std::shared_ptr<BlockWorker> BlockWorkerPtr;
}
// namespace anonymous

void
GossCmdComputeNearKmers::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    Timer t;

    KmerSet s(mIn, fac);

    dynamic_bitset<> lb(s.count());
    dynamic_bitset<> rb(s.count());

    boost::atomic<uint64_t> global(0);
    boost::atomic<uint64_t> gray(0);
    {
        WordyBitVector lhs(mIn + ".lhs-bits", fac);
        WordyBitVector rhs(mIn + ".rhs-bits", fac);

        // Initialise lb and rb.
        log(info, "initialising bitsets");
        for (uint64_t i = 0; i < s.count(); ++i)
        {
            lb[i] = lhs.get(i);
            rb[i] = rhs.get(i);
        }

        log(info, "calculating grey set");
        ProgressMonitorNew pm(log, s.count());
        //ProgressUpdater u(log, grayCount, totalCount);
        vector<BlockWorkerPtr> workers;
        uint64_t d = s.count() / mNumThreads;
        mutex mut;
        condition_variable cond;
        uint64_t m = (1ULL << 20) - 1;
        for (uint64_t i = 0; i < mNumThreads; ++i)
        {
            uint64_t b = i * d;
            uint64_t e = (i + 1) * d;
            if (i == mNumThreads - 1)
            {
                e = s.count();
            }
            workers.push_back(BlockWorkerPtr(new BlockWorker(s, lhs, rhs, b, e, mut, cond, lb, rb, global, gray, m)));
        }

        ThreadGroup grp;
        for (uint64_t i = 0; i < workers.size(); ++i)
        {
            grp.create(*workers[i]);
        }
        {
            unique_lock<mutex> lk(mut);
            while (global < s.count())
            {
                cond.wait(lk);
                mut.unlock();
                double q = global * 100;
                q /= s.count();
                mut.lock();
            }
        }
        grp.join();
    }

    log(info, "found " + lexical_cast<string>(gray) + " gray bits (out of " + lexical_cast<string>(s.count()) + ").");
    WordyBitVector::Builder lhsBld(mIn + ".lhs-bits", fac);
    WordyBitVector::Builder rhsBld(mIn + ".rhs-bits", fac);
    for (uint64_t i = 0; i < s.count(); ++i)
    {
        lhsBld.push_backX(lb[i]);
        rhsBld.push_backX(rb[i]);
    }
    lhsBld.end();
    rhsBld.end();

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryComputeNearKmers::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    uint64_t t = 4;
    chk.getOptional("num-threads", t);
    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdComputeNearKmers(in, t));
}

GossCmdFactoryComputeNearKmers::GossCmdFactoryComputeNearKmers()
    : GossCmdFactory("Decorate a graph with an assignment of kmers to graphs.")
{
    mCommonOptions.insert("graph-in");
}
