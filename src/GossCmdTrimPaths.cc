// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdTrimPaths.hh"

#include "Debug.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "Timer.hh"
#include "ProgressMonitor.hh"
#include "ThreadGroup.hh"

#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/mpl/numeric_cast.hpp>

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
            mOK |= mGraph.multiplicity(pRank) >= mMinCount;
            return mOK;
        }

        Vis(const Graph& pGraph, uint32_t pMinCount, vector<EdgeAndRank>& pEdges)
            : mGraph(pGraph), mEdges(pEdges), mMinCount(pMinCount), mOK(true)
        {
        }

        bool ok() const
        {
            return mOK;
        }

    private:
        const Graph& mGraph;
        vector<EdgeAndRank>& mEdges;
        uint32_t mMinCount;
        bool mOK;
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

    class Block
    {
    public:
        void operator()()
        {
            vector<EdgeAndRank> edges;
            vector<EdgeAndRank> otherEdges;
            vector<uint64_t> zapRanks;

            for (uint64_t i = mBegin; i < mEnd; ++i)
            {
                Graph::Edge beg = mGraph.select(i);
                Graph::Node n = mGraph.from(beg);
                if (mGraph.inDegree(n) != 0)
                {
                    continue;
                }

                edges.clear();
                Vis vis(mGraph, mC, edges);
                Graph::Edge end = mGraph.linearPath(beg, vis);

                if (0)
                {
                    otherEdges.clear();
                    Vis otherVis(mGraph, mC, otherEdges);
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

                // Okay, we've got a low-coverage edge.

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
                ++mPathCount;
                mZapCount += zapRanks.size();
            }
        }

        Block(const Graph& pGraph, dynamic_bitset<>& pZapped, std::mutex& pMutex,
              uint64_t& pTipCount, uint64_t& pZapCount, uint32_t pC,
              uint64_t pBegin, uint64_t pEnd)
            : mGraph(pGraph), mZapped(pZapped), mMutex(pMutex),
              mPathCount(pTipCount), mZapCount(pZapCount), mC(pC),
              mBegin(pBegin), mEnd(pEnd)
        {
        }

    private:
        const Graph& mGraph;
        dynamic_bitset<>& mZapped;
        std::mutex& mMutex;
        uint64_t& mPathCount;
        uint64_t& mZapCount;
        const uint32_t mC;
        const uint64_t mBegin;
        const uint64_t mEnd;
    };
    typedef std::shared_ptr<Block> BlockPtr;

} // namespace anonymous


void
GossCmdTrimPaths::operator()(const GossCmdContext& pCxt)
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

    dynamic_bitset<> zapped(g.count());

    std::mutex mtx;

    uint64_t zapCount = 0;
    uint64_t pathCount = 0;

    log(info, "locating low-coverage paths");
    Timer t;

    uint64_t N = g.count();
    uint64_t J = mThreads;
    uint64_t S = N / J;
    vector<BlockPtr> blks;
    for (uint64_t i = 0; i < J; ++i)
    {
        uint64_t b = i * S;
        uint64_t e = (i == J - 1 ? N : (i + 1) * S);
        if (b != e)
        {
            blks.push_back(BlockPtr(new Block(g, zapped, mtx, pathCount, zapCount, mC, b, e)));
        }
    }
    ThreadGroup grp;
    for (uint64_t i = 0; i < J; ++i)
    {
        grp.create(*blks[i]);
    }
    grp.join();

    log(info, "writing out graph.");
    ProgressMonitor writeMon(log, g.count(), 200);
    Graph::Builder b(g.K(), mOut, fac, g.count() - zapCount);
    uint64_t i = 0;
    for (Graph::Iterator itr(g); itr.valid(); ++itr, ++i)
    {
        writeMon.tick(i);
        Graph::Edge e = (*itr).first;
        uint32_t c = (*itr).second;
        if (0)
        {
            Graph::Edge ep = g.reverseComplement(e);
            uint64_t r = g.rank(ep);
            BOOST_ASSERT(zapped[i] == zapped[r]);
            BOOST_ASSERT(c == g.multiplicity(r));
        }
        if (zapped[i])
        {
            continue;
        }
        BOOST_ASSERT(c > 0);
        b.push_back(e.value(), c);
    }
    b.end();

    log(info, "number of paths removed: " + lexical_cast<string>(pathCount));
    log(info, "number of edges removed: " + lexical_cast<string>(zapCount));
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryTrimPaths::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    FileFactory& fac(pApp.fileFactory());
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    uint64_t c = 0;
    chk.getMandatory("cutoff", c);

    uint64_t t = 4;
    chk.getOptional("num-threads", t);
    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdTrimPaths(in, out, numeric_cast<uint32_t>(c), t));
}

GossCmdFactoryTrimPaths::GossCmdFactoryTrimPaths()
    : GossCmdFactory("create a new graph by removing low frequency paths")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("cutoff");
}
