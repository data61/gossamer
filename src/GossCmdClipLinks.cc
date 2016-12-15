// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdClipLinks.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "ProgressMonitor.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace {

class EdgeVisitor 
{
public:

    bool operator()(const Graph::Edge& pEdge, const Gossamer::rank_type pRank)
    {
        mRanks.push_back(pRank);
        mSeen[pRank] = true;
        return true;
    }

    EdgeVisitor(vector<Gossamer::rank_type>& pRanks, dynamic_bitset<>& pSeen) :
        mRanks(pRanks), mSeen(pSeen)
    {
    }

private:

    vector<Gossamer::rank_type>& mRanks;
    dynamic_bitset<>& mSeen;
};

bool minorOut(Graph& pG, Graph::Node& pN, uint32_t pC, double pThresh)
{
    // Find sum of coverage of outgoing edges of a node.
    uint32_t c_out_sum = 0;
    pair<Gossamer::rank_type, Gossamer::rank_type> rs(pG.beginEndRank(pN));
    for (Gossamer::rank_type r = rs.first; r != rs.second; ++r)
    {
        c_out_sum += pG.multiplicity(r);
    }

    return double(pC) / double(c_out_sum) < pThresh;
}

bool minorIn(Graph& pG, Graph::Node& pN, uint32_t pC, double pThresh)
{
    
    // Find sum of coverage of incoming edges of a node.
    uint32_t c_in_sum = 0;
    pair<Gossamer::rank_type, Gossamer::rank_type> rs(pG.beginEndRank(pG.reverseComplement(pN)));
    for (Gossamer::rank_type r = rs.first; r != rs.second; ++r)
    {
        Graph::Edge e(pG.reverseComplement(pG.select(r)));
        c_in_sum += pG.multiplicity(e);
    }

    return double(pC) / double(c_in_sum) < pThresh;
}
    
Gossamer::rank_type rcRank(Graph& pG, Gossamer::rank_type pR)
{
    Graph::Edge e(pG.select(pR)); 
    e = pG.reverseComplement(e);
    return pG.rank(e);
}

};

void
GossCmdClipLinks::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    log(info, "loading graph");
    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }
    
    const uint64_t k = g.K();
    const uint64_t z = g.count();

    dynamic_bitset<> seen(z);
    dynamic_bitset<> zap(z);
    vector<Gossamer::rank_type> ranks;
    EdgeVisitor vis(ranks, seen);
    uint64_t numZapped = 0;

    // TODO: Make this configurable.
    const double thresh(1.0 / 3.0);
    const uint32_t minLen(2 * g.K());
    uint32_t linksZapped = 0;
    uint32_t edgesZapped = 0;
 
    Timer t;
    ProgressMonitorNew mon1(log, z);
    log(info, "scanning for spurious links");
    for (uint64_t i = 0; i < z; ++i)
    {
        mon1.tick(i);
        if (seen[i])
        {
            continue;
        }

        Graph::Edge e = g.select(i);
        Graph::Node e_f = g.from(e);
        if (g.outDegree(e_f) == 1)
        {
            continue;
        }

        ranks.clear();
        g.linearPath(e, vis);
        Graph::Node e_t = g.to(g.select((ranks.back())));
        uint32_t c_f = g.multiplicity(i);
        uint32_t c_t = g.multiplicity(ranks.back());

        if (   minorOut(g, e_f, c_f, thresh)
            && minorIn(g, e_t, c_t, thresh)
            && ranks.size() <= minLen)
        {
            // Remove e.
            linksZapped += 1;
            edgesZapped += ranks.size();
            for (uint64_t j = 0; j < ranks.size(); ++j)
            {
                zap[ranks[j]] = true;
                zap[rcRank(g, ranks[j])] = true;
            }
        }
    }

    log(info, "removing spurious links");
    ProgressMonitorNew mon2(log, z);
    const uint64_t n = z - numZapped;
    Graph::Builder b(k, mOut, fac, n);
    for (uint64_t i = 0; i < n; ++i)
    {
        mon2.tick(i);
        if (!zap[i])
        {
            b.push_back(g.select(i).value(), g.multiplicity(i));
        }
    }
    b.end();
    
    log(info, "links removed: " + lexical_cast<string>(linksZapped));
    log(info, "edges removed: " + lexical_cast<string>(edgesZapped));
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryClipLinks::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    FileFactory& fac(pApp.fileFactory());
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdClipLinks(in, out));
}

GossCmdFactoryClipLinks::GossCmdFactoryClipLinks()
    : GossCmdFactory("create a new graph by removing spurious links")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
}
