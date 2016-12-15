// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "TransCmdTrimRelative.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "ProgressMonitor.hh"
#include "MultithreadedBatchTask.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;


struct TransCmdTrimRelative : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    TransCmdTrimRelative(const std::string& pIn, const std::string& pOut,
                         double pRelCoverage, uint64_t pNumThreads)
        : mIn(pIn), mOut(pOut), mRelCoverage(pRelCoverage),
          mNumThreads(pNumThreads)
    {
    }

    struct TrimThread : public MultithreadedBatchTask::WorkThread
    {
        const Graph& mGraph;
        const uint64_t mBegin;
        const uint64_t mEnd;
        const double mRelCoverage;
        vector<uint64_t> mEdgesToCull;
        mutex& mMutex;
        dynamic_bitset<uint64_t>& mEdgesToRemove;

        virtual void operator()();

        TrimThread(const Graph& pGraph, uint64_t pBegin, uint64_t pEnd,
                   double pRelCoverage,
                   mutex& pMutex, dynamic_bitset<uint64_t>& pEdgesToRemove,
                   MultithreadedBatchTask& pTask);

        void processNode(uint64_t pNodeBegin, uint64_t pNodeEnd,
                         const vector<uint32_t>& pCounts);
    };

private:
    const std::string mIn;
    const std::string mOut;
    const double mRelCoverage;
    const uint64_t mNumThreads;
};


TransCmdTrimRelative::TrimThread::TrimThread(
        const Graph& pGraph, uint64_t pBegin, uint64_t pEnd,
        double pRelCoverage,
        mutex& pMutex, dynamic_bitset<uint64_t>& pEdgesToRemove,
        MultithreadedBatchTask& pTask)
    : MultithreadedBatchTask::WorkThread(pTask),
      mGraph(pGraph), mBegin(pBegin), mEnd(pEnd), mRelCoverage(pRelCoverage),
      mMutex(pMutex), mEdgesToRemove(pEdgesToRemove)
{
}


void
TransCmdTrimRelative::TrimThread::processNode(
        uint64_t pNodeBegin, uint64_t pNodeEnd,
        const vector<uint32_t>& pCounts)
{
    mEdgesToCull.clear();

    uint64_t totalCount = 0; 
    for (auto c: pCounts)
    {
        totalCount += c;
    }

    double thresh = totalCount * mRelCoverage;

    for (size_t i = 0; i < pCounts.size(); ++i)
    {
        if (pCounts[i] < thresh)
        {
            mEdgesToCull.push_back(pNodeBegin + i);
        }
    }

    if (mEdgesToCull.empty())
    {
        return;
    }

    uint64_t edgesToCullSize = mEdgesToCull.size();
    for (size_t i = 0; i < edgesToCullSize; ++i)
    {
        Graph::Edge e = mGraph.select(mEdgesToCull[i]);
        mEdgesToCull.push_back(mGraph.rank(mGraph.reverseComplement(e)));
    }

    std::unique_lock<std::mutex> lock(mMutex);
    for (auto e: mEdgesToCull)
    {
        mEdgesToRemove[e] = true;
    }
}


void
TransCmdTrimRelative::TrimThread::operator()()
{
    uint64_t i = mBegin;

    const int64_t workQuantum = 10000ll * numThreads();
    int64_t workUntilReport = workQuantum;
    vector<uint32_t> counts;
    counts.reserve(4);

    mEdgesToCull.reserve(8);

    // Get the first node.
    uint64_t nodeBegin = i;
    uint64_t nodeEnd = i;
    uint32_t curCount = mGraph.weight(i);
    counts.push_back(curCount);
    Graph::Node curNode = mGraph.from(mGraph.select(i));
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

        uint64_t curRank = i++;
    
        curCount = mGraph.weight(curRank);
        nodeEnd = curRank;
    
        Graph::Node nextNode = mGraph.from(mGraph.select(curRank));
    
        if (nextNode == curNode)
        {
            counts.push_back(curCount);
            continue;
        }
    
        // Only one out edge.
        if (counts.size() <= 1)
        {
            curNode = nextNode;
            nodeBegin = nodeEnd;

            counts.clear();
            counts.push_back(curCount);
            continue;
        }
    
        processNode(nodeBegin, nodeEnd, counts);
    
        curNode = nextNode;
        nodeBegin = nodeEnd;
        counts.clear();
        counts.push_back(curCount);
    }
    
    if (counts.size() > 1)
    {
        processNode(nodeBegin, nodeEnd, counts);
    }
}


void
TransCmdTrimRelative::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);

    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("Asymmetric graphs NYI"));
    }

    log(info, "locating edges to trim");

    Timer t;
    ProgressMonitorNew mon(log, g.count());

    typedef std::shared_ptr<TrimThread> TrimThreadPtr;
    vector<TrimThreadPtr> threads;

    dynamic_bitset<uint64_t> zapped(g.count());
    std::mutex mtx;

    {
        uint64_t N = g.count();
        uint64_t J = mNumThreads;
        uint64_t S = N / J;
        vector<uint64_t> threadStarts;
        threadStarts.reserve(J + 1);

        for (uint64_t i = 0; i < J; ++i)
        {
            uint64_t b = i * S;
            Graph::Node n = g.from(g.select(b));
            uint64_t eRank = g.beginRank(n);
            threadStarts.push_back(eRank);
        }
        threadStarts.push_back(N);

        ProgressMonitorNew mon(log, N);
        MultithreadedBatchTask task(mon);

        for (uint64_t i = 0; i < threadStarts.size() - 1; ++i)
        {
            uint64_t b = threadStarts[i];
            uint64_t e = threadStarts[i+1];
            if (b != e)
            {
                TrimThreadPtr thr(
                    new TrimThread(g, b, e, mRelCoverage, mtx, zapped, task));
                threads.push_back(thr);
                task.addThread(thr);
            }
        }

        task();
    }

    uint64_t zappedCount = zapped.count();
    uint64_t newCount = g.count() - zappedCount;
    log(info, "number of edges removed: " + lexical_cast<string>(zappedCount));

    log(info, "writing out graph.");

    ProgressMonitorNew writeMon(log, g.count());
    Graph::Builder b(g.K(), mOut, fac, newCount, g.asymmetric());

    uint64_t i = 0;
    for (Graph::Iterator itr(g); itr.valid(); ++itr, ++i)
    {
        writeMon.tick(i);

        if (!zapped[i])
        {
            Graph::Edge e = (*itr).first;
            uint32_t c = (*itr).second;
            b.push_back(e.value(), c);
        }
    }

    b.end();
    writeMon.end();

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}



GossCmdPtr
TransCmdFactoryTrimRelative::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileCreateCheck createChk(fac,true);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    chk.getMandatory("graph-out", out, createChk);

    double relCutoff = 0.02;
    chk.getOptional("relative-cutoff", relCutoff);

    uint64_t numThreads = 4;
    chk.getOptional("num-threads", numThreads);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new TransCmdTrimRelative(in, out,
            relCutoff, numThreads));
}

TransCmdFactoryTrimRelative::TransCmdFactoryTrimRelative()
    : GossCmdFactory("create a new graph from an existing graph, using coverage information from a reference graph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("relative-cutoff");
}


