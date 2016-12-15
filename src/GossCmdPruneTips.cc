// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdPruneTips.hh"

#include "Debug.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "Timer.hh"
#include "MultithreadedBatchTask.hh"
#include "ProgressMonitor.hh"

#include <string>
#include <boost/atomic.hpp>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;
typedef std::pair<Graph::Edge,Gossamer::rank_type> EdgeAndRank;

namespace // anonymous
{
    class Vis
    {
    public:
        bool operator()(const Graph::Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            mEdges.push_back(EdgeAndRank(pEdge, pRank));
            return true;
        }

        Vis(vector<EdgeAndRank>& pEdges)
            : mEdges(pEdges)
        {
        }

    private:
        vector<EdgeAndRank>& mEdges;
    };

    void linearPath(const Graph& pGraph, const Graph::Edge& pBegin, Vis& pVis)
    {
        Graph::Edge e = pBegin;
        Graph::Node n = pGraph.to(e);
        while (pGraph.inDegree(n) == 1 && pGraph.outDegree(n) == 1)
        {
            Graph::Edge ee = pGraph.onlyOutEdge(n);
            if (ee == pBegin)
            {
                break;
            }

            pVis(e, pGraph.rank(e));
            e = ee;
            n = pGraph.to(e);
        }
        pVis(e, pGraph.rank(e));
    }

    class Block : public MultithreadedBatchTask::WorkThread
    {
    public:
        void operator()()
        {
            vector<EdgeAndRank> edges;
            vector<EdgeAndRank> otherEdges;
            vector<uint64_t> zapRanks;
            int workQuantum = 10000ll;

            bool cutoffCheck = mCutoff.get() > 0;
            bool relCutoffCheck = mRelCutoff.get() > 0;

            for (uint64_t i = mBegin; i < mEnd; ++i)
            {
                if (--workQuantum <= 0)
                {
                    if (!reportWorkDone(i - mBegin))
                    {
                        return;
                    }
                    workQuantum = 10000ll;
                }

                Graph::Edge beg = mGraph.select(i);
                Graph::Node n = mGraph.from(beg);
                if (mGraph.inDegree(n) != 0)
                {
                    continue;
                }

                edges.clear();
                Vis vis(edges);
                Graph::Edge end = mGraph.linearPath(beg, vis);

                if (0)
                {
                    otherEdges.clear();
                    Vis otherVis(otherEdges);
                    linearPath(mGraph, beg, otherVis);

                    if (edges != otherEdges)
                    {
                        for (uint64_t j = 0; j < otherEdges.size(); ++j)
                        {
                            cerr << otherEdges[j].second << '\t';
                        }
                        cerr << endl;
                        for (uint64_t j = 0; j < edges.size(); ++j)
                        {
                            cerr << edges[j].second << '\t';
                        }
                        cerr << endl;
                        throw "splat";
                    }
                }

                uint64_t l = edges.size();
                if (l > 2 * mGraph.K())
                {
                    continue;
                }

                uint8_t begIn = mGraph.inDegree(mGraph.from(beg));
                uint8_t begOut = mGraph.outDegree(mGraph.from(beg));
                uint8_t endIn = mGraph.inDegree(mGraph.to(end));
                uint8_t endOut = mGraph.outDegree(mGraph.to(end));

                bool begCon = begOut > 1 || begIn > 0;
                bool endCon = endIn > 1 || endOut > 0;

                // If both ends are connected, it can't be a tip.
                if (begCon && endCon)
                {
                    continue;
                }

                // Okay, we've got a tip.

                uint32_t c = 0;
                bool check = false;
                if (!begCon && endCon)
                {
                    // Joined at the end
                    c  = mGraph.multiplicity(end);
                    n = mGraph.reverseComplement(mGraph.to(end));
                    check = true;
                }
                else if (!endCon && begCon)
                {
                    // Joined at the beginning
                    c  = mGraph.multiplicity(beg);
                    n = mGraph.from(beg);
                    check = true;
                }
                else
                {
                    // Not joined at all!
                    BOOST_ASSERT(!begCon && !endCon);
                    continue;
                }

                // Perform cutoff check
                if (cutoffCheck && c < mRelCutoff.get())
                {
                    continue;
                }

                BOOST_ASSERT(check);

                {
                    // Check that there are no edges from the
                    // attaching node with lower coverage than
                    // this node, and that the edge passes the
                    // relative cutoff threshold (if applicable).
                    pair<uint64_t,uint64_t> r = mGraph.beginEndRank(n);
                    bool okay = true;
                    uint32_t totalCoverage = 0;
                    for (uint64_t j = r.first; j < r.second; ++j)
                    {
                        uint32_t cov = mGraph.multiplicity(j);
                        totalCoverage += cov;

                        if (cov < c)
                        {
                            okay = false;
                            break;
                        }
                    }

                    if (!okay ||
                      (relCutoffCheck && c < totalCoverage * mRelCutoff.get()))
                    {
                        continue;
                    }
                }

                //log(info, "Zap!");
                zapRanks.clear();
                for (uint64_t j = 0; j < edges.size(); ++j)
                {
                    Graph::Edge x = edges[j].first;
                    Graph::Edge y = mGraph.reverseComplement(x);
                    uint64_t x_rnk = edges[j].second;
                    uint64_t y_rnk = mGraph.rank(y);
                    zapRanks.push_back(x_rnk);
                    zapRanks.push_back(y_rnk);
                }

                std::unique_lock<std::mutex> lk(mMutex);
                for (uint64_t j = 0; j < zapRanks.size(); ++j)
                {
                    mZapped[zapRanks[j]] = true;
                }
                ++mTipCount;
                mZapCount += zapRanks.size();
            }

            reportWorkDone(mEnd - mBegin);
        }

        Block(MultithreadedBatchTask& pTask, const Graph& pGraph,
              dynamic_bitset<>& pZapped, std::mutex& pMutex,
              boost::atomic<uint64_t>& pTipCount, boost::atomic<uint64_t>& pZapCount,
              uint64_t pBegin, uint64_t pEnd,
              const optional<uint64_t> pCutoff,
              const optional<double> pRelCutoff)
            : MultithreadedBatchTask::WorkThread(pTask),
              mGraph(pGraph), mZapped(pZapped), mMutex(pMutex),
              mTipCount(pTipCount), mZapCount(pZapCount),
              mBegin(pBegin), mEnd(pEnd), mCutoff(pCutoff), mRelCutoff(pRelCutoff)
        {
        }

    private:
        const Graph& mGraph;
        dynamic_bitset<>& mZapped;
        std::mutex& mMutex;
        boost::atomic<uint64_t>& mTipCount;
        boost::atomic<uint64_t>& mZapCount;
        const uint64_t mBegin;
        const uint64_t mEnd;
        const optional<uint64_t> mCutoff;
        const optional<double> mRelCutoff;
    };
    typedef std::shared_ptr<Block> BlockPtr;

    Debug dumpGraphBuildStats("dump-graph-build-stats", "Dump the graph builder stats.");

} // namespace anonymous


void
GossCmdPruneTips::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }

    Timer t;
    uint64_t zc = 0;
    uint64_t tc = 0;
    for (uint64_t iteration = 0; iteration < mIterations; ++iteration)
    {
        dynamic_bitset<> zapped(g.count());
        std::mutex mtx;

        boost::atomic<uint64_t> zapCount(0);
        boost::atomic<uint64_t> tipCount(0);

        log(info, "locating tips (iteration " + lexical_cast<string>(iteration + 1) + ")");

        uint64_t N = g.count();
        uint64_t J = mThreads;
        uint64_t S = N / J;

        vector<BlockPtr> blks;
        {
            ProgressMonitorNew mon(log, N);
            MultithreadedBatchTask task(mon);

            for (uint64_t i = 0; i < J; ++i)
            {
                uint64_t b = i * S;
                uint64_t e = (i == J - 1 ? N : (i + 1) * S);
                if (b != e)
                {
                    BlockPtr blk(new Block(task, g, zapped, mtx,
                        tipCount, zapCount, b, e,
                        mCutoff, mRelCutoff));
                    task.addThread(blk);
                    blks.push_back(blk);
                }
            }
            task();
        }

        g.remove(zapped);
        log(info, "number of tips removed: " + lexical_cast<string>(tipCount));
        log(info, "number of edges removed: " + lexical_cast<string>(zapCount));
        tc += tipCount;
        zc += zapCount;
    }

    log(info, "writing out graph.");
    ProgressMonitorNew writeMon(log, g.count());
    Graph::Builder b(g.K(), mOut, fac, g.count());
    uint64_t i = 0;
    for (Graph::Iterator itr(g); itr.valid(); ++itr)
    {
        writeMon.tick(++i);
        Graph::Edge e = (*itr).first;
        uint32_t c = (*itr).second;
        b.push_back(e.value(), c);
    }
    b.end();
    writeMon.end();

    if (dumpGraphBuildStats.on())
    {
        PropertyTree pt(b.stat());
        pt.print(cerr);
    }

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
    log(info, "total number of tips removed: " + lexical_cast<string>(tc));
    log(info, "total number of edges removed: " + lexical_cast<string>(zc));
}


GossCmdPtr
GossCmdFactoryPruneTips::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    FileFactory& fac(pApp.fileFactory());
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    uint64_t I = 1;
    chk.getOptional("iterate", I);

    optional<uint64_t> c;
    chk.getOptional("cutoff", c);

    optional<double> relC;
    chk.getOptional("relative-cutoff", relC);

    uint64_t t = 4;
    chk.getOptional("num-threads", t);
    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdPruneTips(in, out, c, relC, t, I));
}

GossCmdFactoryPruneTips::GossCmdFactoryPruneTips()
    : GossCmdFactory("create a new graph by removing low frequency tips")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("cutoff");
    mCommonOptions.insert("relative-cutoff");
    mCommonOptions.insert("iterate");
}

