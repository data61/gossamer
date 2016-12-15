// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildSubgraph.hh"

#include "LineSource.hh"
#include "EdgeCompiler.hh"
#include "ExternalSort64.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "Logger.hh"
#include "ReadSequenceFileSequence.hh"
#include "ReverseComplementAdapter.hh"
#include "Timer.hh"

#include <string>
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

    struct SingleFollower
    {
        static void follow(const Graph& pGraph,
                           dynamic_bitset<>& pInteresting, dynamic_bitset<>& pFringe,
                           const Graph::Edge& pEdge)
        {
            pair<uint64_t,uint64_t> r = pGraph.beginEndRank(pGraph.to(pEdge));
            for (uint64_t k = r.first; k < r.second; ++k)
            {
                pFringe[k] = !pInteresting[k];
                Graph::Edge f = pGraph.select(k);
                Graph::Edge f_rc = pGraph.reverseComplement(f);
                uint64_t k_rc = pGraph.rank(f_rc);
                pFringe[k_rc] = !pInteresting[k_rc];
            }
        }
    };

    struct SegmentFollower
    {
        static void follow(const Graph& pGraph,
                           dynamic_bitset<>& pInteresting, dynamic_bitset<>& pFringe,
                           const Graph::Edge& pEdge)
        {
            vector<EdgeAndRank> ers;
            Vis v(ers);
            Graph::Edge e = pGraph.linearPath(pEdge, v);
            for (uint64_t i = 0; i < ers.size(); ++i)
            {
                pInteresting[ers[i].second] = true;
                Graph::Edge f = pGraph.reverseComplement(ers[i].first);
                uint64_t r = pGraph.rank(f);
                pInteresting[r] = true;
            }
            SingleFollower::follow(pGraph, pInteresting, pFringe, e);
        }
    };


    template <typename Scanner>
    void scanGraph(const Graph& pGraph, uint64_t pRadius, dynamic_bitset<>& pInteresting, Logger& pLog)
    {
        dynamic_bitset<> prev(pInteresting);
        dynamic_bitset<> fringe(pGraph.count());
        for (uint64_t i = 0; i < pRadius; ++i)
        {
            uint64_t n = pInteresting.count();
            fringe.clear();
            fringe.resize(pGraph.count());
            for (uint64_t j = 0; j < prev.size(); ++j)
            {
                // What we really want is a select driven loop!
                if (!prev[j])
                {
                    continue;
                }

                // Add the successor edges
                //
                Graph::Edge e = pGraph.select(j);
                Scanner::follow(pGraph, pInteresting, fringe, e);

                // Add the predecessor edges
                //
                Graph::Edge e_rc = pGraph.reverseComplement(e);
                Scanner::follow(pGraph, pInteresting, fringe, e_rc);
            }
            pInteresting |= fringe;
            n = pInteresting.count() - n;
            prev.swap(fringe);
            pLog(info, "pass " + lexical_cast<string>(i) + " identified " + lexical_cast<string>(n) + " additional edges.");
        }
    }

} // namespace anonymous

void
GossCmdBuildSubgraph::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);
    Timer t;

    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }
    const uint64_t k = g.K();

    std::deque<GossReadSequence::Item> items;

    {
        GossReadSequenceFactoryPtr seqFac
            = std::make_shared<GossReadSequenceBasesFactory>();

        GossReadParserFactory lineParserFac(LineParser::create);
        for (auto& f: mLineNames)
        {
            items.push_back(GossReadSequence::Item(f, lineParserFac, seqFac));
        }

        GossReadParserFactory fastaParserFac(FastaParser::create);
        for (auto& f: mFastaNames)
        {
            items.push_back(GossReadSequence::Item(f, fastaParserFac, seqFac));
        }

        GossReadParserFactory fastqParserFac(FastqParser::create);
        for (auto& f: mFastqNames)
        {
            items.push_back(GossReadSequence::Item(f, fastqParserFac, seqFac));
        }
    }

    UnboundedProgressMonitor umon(log, 100000, " reads");
    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    ReadSequenceFileSequence reads(items, fac, lineSrcFac, &umon);

    ReverseComplementAdapter revs(reads, k + 1);

    dynamic_bitset<> interesting(g.count());

    while (revs.valid())
    {
        Graph::Edge e(*revs);
        uint64_t r = 0;
        if (g.accessAndRank(e, r))
        {
            interesting[r] = true;
        }
        ++revs;
    }
    if (mLinearPaths)
    {
        scanGraph<SegmentFollower>(g, mRadius, interesting, log);
    }
    else
    {
        scanGraph<SingleFollower>(g, mRadius, interesting, log);
    }

    Graph::Builder bld(g.K(), mOut, fac, interesting.count());
    for (uint64_t i = 0; i < interesting.size(); ++i)
    {
        if (interesting[i])
        {
            bld.push_back(g.select(i).value(), uint64_t(g.multiplicity(i)));
        }
    }
    bld.end();

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryBuildSubgraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileCreateCheck createChk(fac, true);
    GossOptionChecker::FileReadCheck readChk(fac);
    
    string out;
    chk.getMandatory("graph-out", out, createChk);

    strings fastaNames;
    chk.getRepeating0("fasta-in", fastaNames, readChk);

    strings fastqNames;
    chk.getRepeating0("fastq-in", fastqNames, readChk);

    strings lineNames;
    chk.getRepeating0("line-in", lineNames, readChk);

    uint64_t radius = 1;
    chk.getOptional("radius", radius);

    bool lp = false;
    chk.getOptional("linear-paths", lp);

    uint64_t B = 2;
    chk.getOptional("buffer-size", B);
    B *= 1024ULL * 1024ULL * 1024ULL;

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildSubgraph(in, out, fastaNames, fastqNames, lineNames, radius, lp, B));
}

GossCmdFactoryBuildSubgraph::GossCmdFactoryBuildSubgraph()
    : GossCmdFactory("generate a subgraph of an existing graph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("buffer-size");

    mSpecificOptions.addOpt<uint64_t>("radius", "", 
                                      "distance of the furthest node to include from any source");
    mSpecificOptions.addOpt<bool>("linear-paths", "",
                                  "interpret radius in terms of linear paths, not nodes");
}
