// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildEdgeIndex.hh"

#include "LineSource.hh"
#include "Debug.hh"
#include "EdgeIndex.hh"
#include "EntryEdgeSet.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "SuperGraph.hh"
#include "SuperPathId.hh"
#include "Timer.hh"

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

void
GossCmdBuildEdgeIndex::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    Timer t;

    LOG(log, info) << "constructing index";
    auto gPtr = Graph::open(mIn, fac);
    auto sgp = SuperGraph::read(mIn, fac);
    const Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }

    const SuperGraph& sg(*sgp);
    const EntryEdgeSet& entries(sg.entries());
    auto ixPtr = EdgeIndex::create(g, entries, sg, mCacheRate, mNumThreads, log);
    LOG(log, info) << "writing index";
    ixPtr->write(mIn, fac);
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryBuildEdgeIndex::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    uint64_t cr = 4;
    chk.getOptional("edge-cache-rate", cr);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildEdgeIndex(in, T, cr));
}

GossCmdFactoryBuildEdgeIndex::GossCmdFactoryBuildEdgeIndex()
    : GossCmdFactory("build an index for aligning pairs to the graph")
{
    mCommonOptions.insert("graph-in");
}
