// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "TourBus.hh"
#include "GraphTrimmer.hh"
#include "ProgressMonitor.hh"
#include "SimpleHashSet.hh"
#include "MultithreadedBatchTask.hh"
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/tuple/tuple_io.hpp>

#undef VERBOSE_DEBUG
#undef TEST_FIB_HEAP
#include "FibHeap.hh"

using namespace std;
using namespace boost;

namespace {
    std::ostream& operator<<(std::ostream& pOut, const std::pair<uint64_t,uint64_t>& pVal)
    { 
        pOut << '(' << pVal.first << ',' << pVal.second << ')';
        return pOut;
    } 
}

struct TourBus::Impl
{
    struct CoverageVisitor
    {
        const Graph& mGraph;
        uint64_t mLength;
        uint64_t mCoverage;

        void reset()
        {
            mLength = 0;
            mCoverage = 0;
        }

        void operator()(const Graph::Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            mCoverage = mGraph.multiplicity(pRank);
            ++mLength;
        }

        CoverageVisitor(const Graph& pGraph)
            : mGraph(pGraph)
        {
            reset();
        }
    };

    void doSingleNode(const Graph::Node& pBegin);

    typedef map<uint64_t,Graph::Edge> path_predecessor_t;
    path_predecessor_t mPredecessors;

    struct LinearPathInfo
    {
        uint64_t mBeginRank;
        Graph::Edge mBegin;
        Graph::Edge mEnd;
        uint64_t mWeight;
        float mTime;
        uint32_t mDistance;

        LinearPathInfo(const Graph& pGraph, uint64_t pBeginRank)
            : mBeginRank(pBeginRank),
              mBegin(pGraph.select(pBeginRank)),
              mEnd(mBegin),
              mWeight(pGraph.weight(pBeginRank))
        {
            Graph::PathCounter acc;
            mEnd = pGraph.linearPath(mBegin, acc);
            mDistance = acc.value();
            mTime = static_cast<float>(static_cast<double>(mDistance)
                        / mWeight);
        }
    };

    void composeSequence(const deque<Graph::Edge>& pSegStarts, SmallBaseVector& pSeq) const;

    void pass();

    void doWholeGraph();

    void doNode(float pTime, uint32_t pDistance, uint64_t pNodeRnk);

    void doPath(float pOriginTime, uint32_t pOriginDistance, const LinearPathInfo& pPath);

    void analyseEdge(const Graph::Edge& pEnd, const Graph::Edge& pBegin);

    bool isOnPredecessorChain(const Graph::Edge& pEnd, const Graph::Edge& pBegin) const;

    uint64_t rank(const Graph::Node& pNode) const
    {
        vector<Graph::Node>::const_iterator i = lower_bound(mNodes.begin(), mNodes.end(), pNode);
        BOOST_ASSERT(i != mNodes.end());
        BOOST_ASSERT(*i == pNode);
        return i - mNodes.begin();
    }

    typedef pair<Graph::Edge,Graph::Edge> Segment;
    typedef pair<uint64_t,Graph::Node> StartNodeItem;
    typedef map<uint64_t,float> dist_map_t;
    typedef boost::tuple<uint64_t,float,uint32_t> WorkItem;

    struct WorkQueue
    {
        typedef pair<uint64_t,uint64_t> value_type;
        typedef FibHeap<float,value_type> fwd_t;
        typedef std::unordered_map<uint64_t,fwd_t::iterator> rev_t;
        fwd_t mFwd;
        rev_t mRev;

#if 0
        void verifyConsistency()
        {
            if (!mFwd.checkHeapInvariant())
            {
                std::cerr << "ERROR: Heap invariant broken\n";
                throw 0;
            }
            fwd_t::Iterator it(mFwd.iterate());
            while (!it.empty())
            {
                fwd_t::const_iterator i = *it;
                rev_t::iterator revit = mRev.find(i->mValue.first);
                if (revit == mRev.end())
                {
                    std::cerr << "ERROR: Can't find " << i->mValue.first <<
                        " in the WorkQueue reverse map.\n";
                    throw 0;
                }
                ++it;
            }
        }
#endif
    
        void clear()
        {
            mFwd.clear();
            mRev.clear();
        }

        bool empty() const
        {
            return mFwd.empty();
        }

        uint64_t size() const
        {
            return mFwd.size();
        }

        void insert(float pTime, uint64_t pNode, uint64_t pDist)
        {
            mRev[pNode] = mFwd.insert(pTime, value_type(pNode, pDist));
        }

        WorkItem get() const
        {
            fwd_t::const_iterator it = mFwd.minimum();
            return WorkItem(it->mValue.first, it->mKey, it->mValue.second);
        }

        void removeMinimum()
        {
            fwd_t::iterator it = mFwd.minimum();
            mRev.erase(mRev.find(it->mValue.first));
            mFwd.removeMinimum();
        }

        void updateValue(uint64_t pNode, float pTime, uint64_t pDist)
        {
            rev_t::const_iterator it = mRev.find(pNode);
            if (it != mRev.end())
            {
                mFwd.decreaseKey(it->second, pTime);
                it->second->mValue.second = pDist;
            }
            else
            {
                mRev[pNode] = mFwd.insert(pTime, value_type(pNode, pDist));
            }
        }
    };

    const Graph& mGraph;
    Logger& mLog;
    GraphTrimmer mTrimmer;
    size_t mMaxSequenceLength;
    size_t mMaxEditDistance;
    double mMaxRelativeErrors;
    bool mDoCutoffCheck;
    uint64_t mCutoff;
    bool mDoRelCutoffCheck;
    double mRelCutoff;
    uint64_t mNumThreads;
    WorkQueue mWorkQueue;
    vector<Graph::Node> mNodes;
    dist_map_t mDistance;
    uint64_t mPotentialBubblesConsidered;
    uint64_t mBubblesRemoved;
    uint64_t mPathsRemoved;
    uint64_t mEdgesRemoved;

    Impl(const Graph& pGraph, Logger& pLog);

    void findStartNodes(deque<StartNodeItem>& pStartNodeQueue);

    struct FindStartNodeThread;
    typedef std::shared_ptr<FindStartNodeThread> FindStartNodeThreadPtr;

    struct FindStartNodeThread : public MultithreadedBatchTask::WorkThread
    {
        uint64_t mThreadId;
        const Graph& mGraph;
        uint64_t mBegin;
        uint64_t mEnd;
        deque<Graph::Node> mNodes;
        deque<StartNodeItem> mStartNodeItems;

        typedef pair<Graph::Edge,uint32_t> count_type;

        void processNode(const Graph::Node pCurNode,
                         const vector<count_type>& pCounts)
        {
            mNodes.push_back(pCurNode);
            mNodes.push_back(mGraph.reverseComplement(pCurNode));

            uint32_t maxMultiplicity = 0;
            for (vector<count_type>::const_iterator ii = pCounts.begin();
                 ii != pCounts.end(); ++ii)
            {
                if (mGraph.to(ii->first) != pCurNode)
                {
                    maxMultiplicity = max(maxMultiplicity, ii->second);
                }
            }
            mStartNodeItems.push_back(StartNodeItem(maxMultiplicity, pCurNode));
        }

        virtual void operator()();

        FindStartNodeThread(int64_t pThreadId, const Graph& pGraph,
                            uint64_t pBegin, uint64_t pEnd,
                            MultithreadedBatchTask& pTask);
    };
};


void
TourBus::Impl::FindStartNodeThread::operator()()
{
    uint64_t i = mBegin;

    const int64_t workQuantum = 10000ll * numThreads();
    int64_t workUntilReport = workQuantum;
    vector<count_type> counts;
    counts.reserve(4);
    
    // Get the first node.
    Graph::Edge curEdge = mGraph.select(i);
    uint32_t curCount = mGraph.weight(i);
    counts.push_back(count_type(curEdge, curCount));
    Graph::Node curNode = mGraph.from(curEdge);
    ++i;
    
    while (i < mEnd)
    {
        if (--workUntilReport <= 0)
        {
            if (!reportWorkDone(i - mBegin))
            {
                return;
            }
            workUntilReport = workQuantum;
        }
    
        curEdge = mGraph.select(i);
        curCount = mGraph.weight(i);
        ++i;
    
        Graph::Node nextNode = mGraph.from(curEdge);
    
        if (nextNode == curNode)
        {
            counts.push_back(count_type(curEdge, curCount));
            continue;
        }
    
        if (counts.size() == 1 && mGraph.inDegree(curNode) == 1)
        {
            curNode = nextNode;
            counts.clear();
    
            counts.push_back(count_type(curEdge, curCount));
            continue;
        }
    
        processNode(curNode, counts);
    
        counts.clear();
        curNode = nextNode;
        counts.push_back(count_type(curEdge, curCount));
    }
    
    if (counts.size() != 1 || mGraph.inDegree(curNode) != 1)
    {
        processNode(curNode, counts);
    }

    sort(mNodes.begin(), mNodes.end());
    mNodes.erase(unique(mNodes.begin(), mNodes.end()), mNodes.end());
    sort(mStartNodeItems.begin(), mStartNodeItems.end());
    reportWorkDone(i - mBegin);
}


TourBus::Impl::FindStartNodeThread::FindStartNodeThread(
                            int64_t pThreadId, const Graph& pGraph,
                            uint64_t pBegin, uint64_t pEnd,
                            MultithreadedBatchTask& pTask)
    : MultithreadedBatchTask::WorkThread(pTask),
      mThreadId(pThreadId), mGraph(pGraph),
      mBegin(pBegin), mEnd(pEnd)
{
}


TourBus::Impl::Impl(const Graph& pGraph, Logger& pLog)
    : mGraph(pGraph),
      mLog(pLog),
      mTrimmer(mGraph)
{
    uint64_t rho = mGraph.K() + 1;
    mMaxSequenceLength = 2 * rho + 2;
    mMaxEditDistance = max<uint64_t>((2 * rho + 27) / 27, 2);
    mMaxRelativeErrors = 0.2;
    mDoCutoffCheck = false;
    mCutoff = 0;
    mDoRelCutoffCheck = false;
    mRelCutoff = 1.0;
    mNumThreads = 1;

    mPotentialBubblesConsidered = 0;
    mBubblesRemoved = 0;
    mPathsRemoved = 0;
    mEdgesRemoved = 0;
}


void
TourBus::Impl::findStartNodes(deque<StartNodeItem>& pStartNodeQueue)
{
    using namespace std::placeholders;

    uint64_t N = mGraph.count();
    uint64_t J = mNumThreads;
    uint64_t S = N / J;
    vector<uint64_t> threadStarts;
    threadStarts.reserve(J + 1);

    for (uint64_t i = 0; i < J; ++i)
    {
        uint64_t b = i * S;
        Graph::Node n = mGraph.from(mGraph.select(b));
        uint64_t eRank = mGraph.beginRank(n);
        threadStarts.push_back(eRank);
    }
    threadStarts.push_back(N);

    vector<FindStartNodeThreadPtr> blocks;
    {
        ProgressMonitorNew mon(mLog, N);
        MultithreadedBatchTask task(mon);

        for (uint64_t i = 0; i < threadStarts.size() - 1; ++i)
        {
            uint64_t b = threadStarts[i];
            uint64_t e = threadStarts[i+1];
            if (b != e)
            {
                FindStartNodeThreadPtr blk(
                    new FindStartNodeThread(i, mGraph, b, e, task));
                blocks.push_back(blk);
                task.addThread(blk);
            }
        }

        mLog(info, "Pass 1: Locating start nodes.");
        task();
    }

    mLog(info, "Done.");

    deque<deque<Graph::Node> > nodeRuns;
    deque<deque<StartNodeItem> > startNodeRuns;
    for (uint64_t i = 0; i < blocks.size(); ++i)
    {
        FindStartNodeThread* blk = blocks[i].get();
        nodeRuns.push_back(deque<Graph::Node>());
        blk->mNodes.swap(nodeRuns.back());
        startNodeRuns.push_back(deque<StartNodeItem>());
        startNodeRuns.back().swap(blk->mStartNodeItems);
    }

    while (nodeRuns.size() > 2)
    {
        deque<Graph::Node> a;
        a.swap(nodeRuns.front());
        nodeRuns.pop_front();
        deque<Graph::Node> b;
        b.swap(nodeRuns.front());
        nodeRuns.pop_front();

        // cast to a specific overload function of push_back
        // this is mainly due to:
        //   deque<T>::push_back(const T&)
        //   deque<T>::push_back(T&&) // C++11 move semantic
        // being unresolvable.
        void (deque<Graph::Node>:: *dequePushBaskConstRef)(const Graph::Node&) = &deque<Graph::Node>::push_back;

        deque<Graph::Node> c;
        merge(a.begin(), a.end(), b.begin(), b.end(),
              make_function_output_iterator(
                  std::bind(dequePushBaskConstRef, std::ref(c), _1)));

        nodeRuns.push_back(deque<Graph::Node>());
        nodeRuns.back().swap(c);
    }

    mNodes.clear();

    switch (nodeRuns.size())
    {
        case 0:
        {
            break;
        }

        case 1:
        {
            deque<Graph::Node> a;
            a.swap(nodeRuns.front());
            nodeRuns.pop_front();
            mNodes.reserve(a.size());
            mNodes.insert(mNodes.end(), a.begin(), a.end());
            break;
        }

        case 2:
        {
            deque<Graph::Node> a;
            a.swap(nodeRuns.front());
            nodeRuns.pop_front();

            deque<Graph::Node> b;
            b.swap(nodeRuns.front());
            nodeRuns.pop_front();

            // cast to a specific overload function of push_back
            // this is mainly due to:
            //   vector<T>::push_back(const T&)
            //   vector<T>::push_back(T&&) // C++11 move semantic
            // being unresolvable.
            void (vector<Graph::Node>:: *vectorPushBaskConstRef)(const Graph::Node&) = &vector<Graph::Node>::push_back;

            mNodes.reserve(a.size() + b.size());
            merge(a.begin(), a.end(), b.begin(), b.end(),
                  make_function_output_iterator(
                      std::bind(vectorPushBaskConstRef, std::ref(mNodes), _1)));
            break;
        }

        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Internal error in TourBus merge phase: node runs = "
                                                    + boost::lexical_cast<std::string>(nodeRuns.size())));
        }
    }

    mNodes.erase(unique(mNodes.begin(), mNodes.end()), mNodes.end());

    while (startNodeRuns.size() > 1)
    {
        deque<StartNodeItem> a;
        a.swap(startNodeRuns.front());
        startNodeRuns.pop_front();
        deque<StartNodeItem> b;
        b.swap(startNodeRuns.front());
        startNodeRuns.pop_front();

        // cast to a specific overload function of push_back
        // this is mainly due to:
        //   vector<T>::push_back(const T&)
        //   vector<T>::push_back(T&&) // C++11 move semantic
        // being unresolvable.
        void (deque<StartNodeItem>:: *dequePushBaskConstRef)(const StartNodeItem&) = &deque<StartNodeItem>::push_back;

        deque<StartNodeItem> c;
        merge(a.begin(), a.end(), b.begin(), b.end(),
              make_function_output_iterator(
                  std::bind(dequePushBaskConstRef, std::ref(c), _1)));

        startNodeRuns.push_back(deque<StartNodeItem>());
        startNodeRuns.back().swap(c);
    }

    pStartNodeQueue.clear();
    switch (startNodeRuns.size())
    {
        case 0:
        {
            break;
        }

        case 1:
        {
            pStartNodeQueue.swap(startNodeRuns.front());
            startNodeRuns.pop_front();
            break;
        }

        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Internal error in TourBus merge phase: start node runs = "
                                                    + boost::lexical_cast<std::string>(startNodeRuns.size())));
        }
    }
}


void
TourBus::Impl::doWholeGraph()
{
    mLog(info, "   max sequence length = "
            + lexical_cast<string>(mMaxSequenceLength));
    mLog(info, "   max edit distance = "
            + lexical_cast<string>(mMaxEditDistance));
    mLog(info, "   max relative error rate = "
            + lexical_cast<string>(mMaxRelativeErrors));
    if (mDoCutoffCheck)
    {
        mLog(info, "   cutoff = " + lexical_cast<string>(mCutoff));
    }
    if (mDoRelCutoffCheck)
    {
        mLog(info, "   relative cutoff = " + lexical_cast<string>(mRelCutoff));
    }

    deque<StartNodeItem> startNodeQueue;
    findStartNodes(startNodeQueue);

    uint64_t j = 0;
    const uint64_t maxPasses = 10000ull;

    mLog(info, "Pass 2: Popping bubbles.");
    ProgressMonitorNew examineMon(mLog, startNodeQueue.size());
    std::map<uint64_t,uint64_t> passCountMap;
    while (!startNodeQueue.empty())
    {
        examineMon.tick(j++);
        Graph::Node n = startNodeQueue.back().second;
        uint64_t nRank = rank(n);
        mPredecessors.clear();
        mDistance.clear();
        mWorkQueue.clear();
        mDistance[nRank] = 0;
        startNodeQueue.pop_back();
#ifdef VERBOSE_DEBUG
        cerr << "Inserting initial work queue node " << nRank << "\n";
#endif
        mWorkQueue.insert(0, nRank, 0);
        uint64_t passes = 0ull;
        while (!mWorkQueue.empty())
        {
            WorkItem item = mWorkQueue.get();
            uint64_t nn = item.get<0>();
#ifdef VERBOSE_DEBUG
            cerr << "Getting minimum work queue node " << nn << "\n";
#endif
            float time = item.get<1>();
            float distance = item.get<2>();
#ifdef VERBOSE_DEBUG
            {
                SmallBaseVector v_n;
                mGraph.seq(mNodes[nn], v_n);
                cerr << "Examining node " << v_n << " with time " << time << " and distance " << distance << "\n";
            }
            cerr << "Removing minimum work queue node " << nn << "\n";
#endif
            mWorkQueue.removeMinimum();
            doNode(time, distance, nn);
            if (++passes > maxPasses)
            {
                SmallBaseVector v_n;
                mGraph.seq(n, v_n);
                mLog(warning, "Potential problem with node " + lexical_cast<string>(v_n) + " (rank " + lexical_cast<string>(nRank) + ")");
                mLog(warning, "Processing will take at least " + lexical_cast<string>(passes + mWorkQueue.size()) + " passes.");
                mLog(warning, "Abandoning this node just in case.");
                break;
            }
        }
        ++passCountMap[passes];
    }
#if 0
    mLog(info, "Pass count histogram: ");
    for (std::map<uint64_t,uint64_t>::const_iterator ii = passCountMap.begin();
         ii != passCountMap.end(); ++ii)
    {
        mLog(info, "    " + lexical_cast<string>(ii->first)
                   + "\t" + lexical_cast<string>(ii->second));
    }
#endif
    mDistance.clear();
    mPredecessors.clear();

    mEdgesRemoved = mTrimmer.removedEdgesCount();

    mLog(info, "  potential bubbles considered: " + lexical_cast<string>(mPotentialBubblesConsidered));
    mLog(info, "  bubbles popped: " + lexical_cast<string>(mBubblesRemoved));
    mLog(info, "  paths removed: " + lexical_cast<string>(mPathsRemoved));
    mLog(info, "  edges removed: " + lexical_cast<string>(mEdgesRemoved));
    mLog(info, "Done.");
}


void
TourBus::Impl::doSingleNode(const Graph::Node& pBegin)
{
    deque<StartNodeItem> startNodeQueue;
    findStartNodes(startNodeQueue);
    startNodeQueue.clear();

    uint64_t nn = rank(pBegin);
    mPredecessors.clear();
    mDistance.clear();
    mDistance[nn] = 0;
    mWorkQueue.insert(0, nn, 0);

    while (!mWorkQueue.empty())
    {
        WorkItem item = mWorkQueue.get();
        uint64_t nn = item.get<0>();
        float time = item.get<1>();
        uint32_t distance = item.get<2>();
#ifdef VERBOSE_DEBUG
        {
            Graph::Node n = mNodes[nn];
            SmallBaseVector v_n;
            mGraph.seq(n, v_n);
            cerr << "Examining node " << v_n << " with time " << time << " and distance " << distance << "\n";
        }
#endif // VERBOSE_DEBUG
        mWorkQueue.removeMinimum();
        doNode(time, distance, nn);
    }

    mDistance.clear();
    mPredecessors.clear();
}


void
TourBus::Impl::doNode(float pTime, uint32_t pDistance, uint64_t pNodeRnk)
{
    pair<uint64_t,uint64_t> r = mGraph.beginEndRank(mNodes[pNodeRnk]);
    uint64_t r0 = r.first;
    uint64_t r1 = r.second;
    for (uint64_t i = r0; i < r1; ++i)
    {
        if (mTrimmer.edgeDeleted(i))
        {
            continue;
        }
        LinearPathInfo path(mGraph, i);
        if (path.mBegin == path.mEnd)
        {
            continue;
        }
        doPath(pTime, pDistance, path);
    }
}


void
TourBus::Impl::doPath(float pOriginTime, uint32_t pOriginDistance,
                      const LinearPathInfo& pPath)
{
#ifdef VERBOSE_DEBUG
    {
        SmallBaseVector v_b;
        mGraph.seq(pPath.mBegin, v_b);
        SmallBaseVector v_e;
        mGraph.seq(pPath.mEnd, v_e);
        cerr << "doPath " << v_b << " -> " << v_e << " with time " << pOriginTime << " and distance " << pOriginDistance << "\n";
    }
#endif // VERBOSE_DEBUG
    Graph::Node endNode = mGraph.to(pPath.mEnd);
    uint64_t endNodeRank = rank(endNode);
    path_predecessor_t::iterator predecessor = mPredecessors.find(endNodeRank);
    if (predecessor != mPredecessors.end() && predecessor->second == pPath.mBegin)
    {
#ifdef VERBOSE_DEBUG
        cerr << "This is a loop.\n";
#endif // VERBOSE_DEBUG
        return;
    }
    double edgeTime = pPath.mTime;
    double totalTime = pOriginTime + edgeTime;
    uint32_t edgeDistance = pPath.mDistance;
    uint32_t totalDistance = pOriginDistance + edgeDistance;
#ifdef VERBOSE_DEBUG
    cerr << "Non-loop with edge time " << edgeTime << " (total time " << totalTime << ") and distance " << edgeDistance << " (total distance " << totalDistance << ")\n";
#endif // VERBOSE_DEBUG

    if (totalDistance > mMaxSequenceLength * 2)
    {
#ifdef VERBOSE_DEBUG
        cerr << "Maximum sequence length bound exceeded.\n";
#endif // VERBOSE_DEBUG
        return;
    }

    dist_map_t::iterator destTimeIt = mDistance.find(endNodeRank);

    if (destTimeIt == mDistance.end())
    {
#ifdef VERBOSE_DEBUG
        cerr << "No previous time found.\n";
#endif // VERBOSE_DEBUG
        mDistance[endNodeRank] = totalTime;
#ifdef VERBOSE_DEBUG
        cerr << "Inserting work queue node " << endNodeRank << "\n";
#endif // VERBOSE_DEBUG
        mWorkQueue.insert(totalTime, endNodeRank, totalDistance);
        mPredecessors.insert(pair<uint64_t,Graph::Edge>(endNodeRank, pPath.mBegin));
        return;
    }
    float destTime = destTimeIt->second;
    if (destTime > totalTime)
    {
#ifdef VERBOSE_DEBUG
        cerr << "New shortest time (previous was " << destTime << ")\n";
#endif // VERBOSE_DEBUG
        destTimeIt->second = totalTime;

#ifdef VERBOSE_DEBUG
        cerr << "Updating work queue node " << endNodeRank << "\n";
#endif // VERBOSE_DEBUG
        mWorkQueue.updateValue(endNodeRank, totalTime, totalDistance);
        BOOST_ASSERT(predecessor != mPredecessors.end());
        analyseEdge(pPath.mEnd, predecessor->second);
        predecessor->second = pPath.mBegin;
        return;
    }

    if (destTime == pOriginTime && isOnPredecessorChain(pPath.mEnd, pPath.mBegin))
    {
#ifdef VERBOSE_DEBUG
        cerr << "New equal time.\n";
#endif // VERBOSE_DEBUG
        return;
    }

    analyseEdge(pPath.mEnd, pPath.mBegin);
}


bool
TourBus::Impl::isOnPredecessorChain(const Graph::Edge& pEnd, const Graph::Edge& pBegin)
    const
{
    // TODO This is obviously incorrect, but a conservative approximation.
    return true;
}


void
TourBus::Impl::analyseEdge(const Graph::Edge& pEnd, const Graph::Edge& pBegin)
{
    const Graph& g = mGraph;
    Graph::Node f = g.from(pBegin);
    uint64_t fRank = rank(f);
    Graph::Node t = g.to(pEnd);
    uint64_t tRank = rank(t);

#ifdef VERBOSE_DEBUG
    SmallBaseVector sbv;
    sbv.clear();
    g.seq(f, sbv);
    cerr << "analyseEdge " << sbv << " -> ";
    sbv.clear();
    g.seq(t, sbv);
    cerr << sbv << ';' << endl;
#endif // VERBOSE_DEBUG

    path_predecessor_t::const_iterator x;
    x = mPredecessors.find(tRank);
    if (x == mPredecessors.end())
    {
        // Not joined on to another path yet,
        // so add the node to the set of paths.
#ifdef VERBOSE_DEBUG
        cerr << "No predecessor.  Inserting: ";
        sbv.clear();
        g.seq(t, sbv);
        cerr << sbv << " -> ";
        sbv.clear();
        g.seq(pBegin, sbv);
        cerr << sbv << endl;
#endif // VERBOSE_DEBUG
        if (g.from(pBegin) == t)
        {
#ifdef VERBOSE_DEBUG
            cerr << "Actually, that would create an infinite loop. Let's not do that.\n";
#endif // VERBOSE_DEBUG
            return;
        }

        path_predecessor_t::value_type v(tRank, pBegin);
        mPredecessors.insert(v);
        return;
    }
#ifdef VERBOSE_DEBUG
    cerr << "Predecessor: ";
    sbv.clear();
    g.seq(t, sbv);
    cerr << sbv << " -> ";
    sbv.clear();
    g.seq(x->second, sbv);
    cerr << sbv << endl;
#endif // VERBOSE_DEBUG
    ++mPotentialBubblesConsidered;

    Graph::Edge majEdge = x->second;

    // We have a join!
    // Now we need to compute the nearest common ancestor,
    // compute the hamming distance and
    // see if this path should be eliminated.

#ifdef VERBOSE_DEBUG
    cerr << "Minority path:\n";
#endif // VERBOSE_DEBUG

    // Let's guess that the minority path is shorter, and index it.
    SimpleHashSet<uint64_t> minority;
    Graph::Node n = f;
    uint64_t nRank = fRank;
    x = mPredecessors.find(nRank);
    minority.insert(nRank);
#ifdef VERBOSE_DEBUG
    sbv.clear();
    g.seq(n, sbv);
    cerr << sbv << "\n";
#endif // VERBOSE_DEBUG
    while (x != mPredecessors.end())
    {
        n = g.from(x->second);
        nRank = rank(n);
        if (minority.count(nRank))
        {
#ifdef VERBOSE_DEBUG
            cerr << "Found a cycle.\n";
#endif // VERBOSE_DEBUG
            break;
        }
        minority.insert(nRank);
#ifdef VERBOSE_DEBUG
        sbv.clear();
        g.seq(n, sbv);
        cerr << sbv << "\n";
#endif // VERBOSE_DEBUG
        x = mPredecessors.find(nRank);
    }

#ifdef VERBOSE_DEBUG
    cerr << "Majority path:\n";
#endif // VERBOSE_DEBUG

    // Now let's scan back up the majority path looking for a common element.
    n = g.from(majEdge);
    nRank = rank(n);
    do
    {
#ifdef VERBOSE_DEBUG
        sbv.clear();
        g.seq(n, sbv);
        cerr << sbv << "\n";
#endif // VERBOSE_DEBUG
        if (minority.count(nRank))
        {
            break;
        }
        x = mPredecessors.find(nRank);
        BOOST_ASSERT(x != mPredecessors.end());
        n = g.from(x->second);
        nRank = rank(n);
    } while (x != mPredecessors.end());

#ifdef VERBOSE_DEBUG
    sbv.clear();
    g.seq(n, sbv);
    cerr << "Found.  Common ancestor node is " << sbv << "\n";
#endif // VERBOSE_DEBUG

    // Okay, n is the common ancestor node.
    // Let's compose the edge lists.

    SmallBaseVector minSeq;
    deque<Graph::Edge> min;
    Graph::Edge e = pBegin;
#ifdef VERBOSE_DEBUG
    sbv.clear();
    g.seq(e, sbv);
    cerr << "Minority edge: " << sbv << "\n";
#endif // VERBOSE_DEBUG
    min.push_front(e);
    while (g.from(e) != n)
    {
        BOOST_ASSERT(mPredecessors.find(rank(g.from(e))) != mPredecessors.end());
        e = mPredecessors.find(rank(g.from(e)))->second;
#ifdef VERBOSE_DEBUG
        sbv.clear();
        g.seq(e, sbv);
        cerr << "Minority edge: " << sbv << "\n";
#endif // VERBOSE_DEBUG
        min.push_front(e);
    }
    composeSequence(min, minSeq);
#ifdef VERBOSE_DEBUG
    cerr << "Minority sequence: " << minSeq << "\n";
#endif // VERBOSE_DEBUG

    if (minSeq.size() > mMaxSequenceLength)
    {
#ifdef VERBOSE_DEBUG
        cerr << "Sequence too long.\n";
#endif // VERBOSE_DEBUG
        return;
    }

    SmallBaseVector maxSeq;
    deque<Graph::Edge> max;
    {
        Graph::Edge e = majEdge;
#ifdef VERBOSE_DEBUG
        sbv.clear();
        g.seq(e, sbv);
        cerr << "Majority edge: " << sbv << "\n";
#endif // VERBOSE_DEBUG
        max.push_front(e);
        while (g.from(e) != n)
        {
            BOOST_ASSERT(mPredecessors.find(rank(g.from(e))) != mPredecessors.end());
            e = mPredecessors.find(rank(g.from(e)))->second;
#ifdef VERBOSE_DEBUG
            sbv.clear();
            g.seq(e, sbv);
            cerr << "Majority edge: " << sbv << "\n";
#endif // VERBOSE_DEBUG
            max.push_front(e);
        }
    }
    composeSequence(max, maxSeq);
#ifdef VERBOSE_DEBUG
    cerr << "Majority sequence: " << maxSeq << "\n";
#endif // VERBOSE_DEBUG

    if (maxSeq.size() > mMaxSequenceLength)
    {
#ifdef VERBOSE_DEBUG
        cerr << "Sequence too long.\n";
#endif // VERBOSE_DEBUG
        return;
    }

    if (static_cast<size_t>(std::abs((int64_t)maxSeq.size() - (int64_t)minSeq.size())) > mMaxEditDistance)
    {
#ifdef VERBOSE_DEBUG
        cerr << "Length difference too high.\n";
#endif // VERBOSE_DEBUG
        return;
    }

    size_t editDistance = maxSeq.editDistance(minSeq);
    if (editDistance > mMaxEditDistance)
    {
#ifdef VERBOSE_DEBUG
        cerr << "Edit distance too high.\n";
#endif // VERBOSE_DEBUG
        return;
    }

    double relErrors = (double)editDistance / std::max(minSeq.size(), maxSeq.size());
    if (relErrors > mMaxRelativeErrors)
    {
#ifdef VERBOSE_DEBUG
        cerr << "Relative error rate too high.\n";
#endif // VERBOSE_DEBUG
        return;
    }

    // Perform cutoff checks
    if (mDoCutoffCheck || mDoRelCutoffCheck)
    {
        CoverageVisitor covVisitor(g);

        for (uint64_t i = 0; i < min.size(); ++i)
        {
            Graph::Edge end = g.linearPath(min[i]);
            g.visitPath(min[i], end, covVisitor);
        }
        double minCoverage = (double)covVisitor.mCoverage / covVisitor.mLength;

        if (mDoCutoffCheck && minCoverage < mCutoff)
        {
            return;
        }

        if (mDoRelCutoffCheck)
        {
            covVisitor.reset();

            for (uint64_t i = 0; i < max.size(); ++i)
            {
                Graph::Edge end = g.linearPath(max[i]);
                g.visitPath(max[i], end, covVisitor);
            }
            double maxCoverage
                = (double)covVisitor.mCoverage / covVisitor.mLength;

            if (minCoverage < maxCoverage * mRelCutoff)
            {
                return;
            }
        }
    }

    ++mBubblesRemoved;
    GraphTrimmer::EdgeTrimVisitor trimVisitor(mTrimmer);
    uint64_t r = g.rank(min.front());
    trimVisitor(min.front(), r);
#ifdef VERBOSE_DEBUG
    sbv.clear();
    g.seq(min.front(), sbv);
    cerr << "Trimming " << sbv << "\n";
#endif
    for (uint64_t i = 0; i < min.size(); ++i)
    {
        Graph::Edge end = g.linearPath(min[i]);
        g.visitPath(min[i], end, trimVisitor);
        ++mPathsRemoved;
#ifdef VERBOSE_DEBUG
        sbv.clear();
        g.seq(min[i], sbv);
        cerr << "Trimming linear path starting from " << sbv << "\n";
#endif
    }
}


void
TourBus::Impl::composeSequence(const deque<Graph::Edge>& pSegStarts,
                               SmallBaseVector& pSeq) const
{
    pSeq.clear();
    if (pSegStarts.size() == 0)
    {
        return;
    }
    mGraph.seq(mGraph.from(pSegStarts.front()), pSeq);
    for (uint64_t i = 0; i < pSegStarts.size(); ++i)
    {
        Graph::Edge end = mGraph.linearPath(pSegStarts[i]);
        mGraph.tracePath1(pSegStarts[i], end, pSeq);
    }
}


void
TourBus::Impl::pass()
{
    mPredecessors.clear();
    mDistance.clear();
    mNodes.clear();
    doWholeGraph();
    mPredecessors.clear();
    mDistance.clear();
}


bool
TourBus::pass()
{
    mPImpl->pass();
    return mPImpl->mTrimmer.modified();
}


bool
TourBus::singleNode(const Graph::Node& pNode)
{
    mPImpl->mPredecessors.clear();
    mPImpl->mDistance.clear();
    mPImpl->mNodes.clear();
    mPImpl->doSingleNode(pNode);
    mPImpl->mPredecessors.clear();
    mPImpl->mDistance.clear();
    return mPImpl->mTrimmer.modified();
}


void
TourBus::setNumThreads(size_t pNumThreads)
{
    mPImpl->mNumThreads = pNumThreads;
}


void
TourBus::setMaximumSequenceLength(size_t pMaxSequenceLength)
{
    mPImpl->mMaxSequenceLength = pMaxSequenceLength;
}


void
TourBus::setMaximumEditDistance(size_t pMaxEditDistance)
{
    mPImpl->mMaxEditDistance = pMaxEditDistance;
}


void
TourBus::setMaximumRelativeErrors(double pMaxRelativeErrors)
{
    mPImpl->mMaxRelativeErrors = pMaxRelativeErrors;
}


void
TourBus::setCoverageCutoff(uint64_t pCutoff)
{
    mPImpl->mDoCutoffCheck = true;
    mPImpl->mCutoff = pCutoff;
}


void
TourBus::setCoverageRelativeCutoff(double pRelCutoff)
{
    mPImpl->mDoRelCutoffCheck = true;
    mPImpl->mRelCutoff = pRelCutoff;
}


void
TourBus::writeModifiedGraph(Graph::Builder& pBuilder) const
{
    mPImpl->mTrimmer.writeTrimmedGraph(pBuilder);
}


uint64_t
TourBus::removedEdgesCount() const
{
    return mPImpl->mTrimmer.removedEdgesCount();
}


TourBus::TourBus(Graph& pGraph, Logger& pLog)
    : mPImpl(new Impl(pGraph, pLog))
{
}


