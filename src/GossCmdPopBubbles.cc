// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdPopBubbles.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "Timer.hh"
#include "TourBus.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;


void
GossCmdPopBubbles::operator()(const GossCmdContext& pCxt)
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

    log(info, "scanning to locate bubbles");
    Timer t;

    TourBus tourBus(g, log);

    if (mNumThreads)
    {
        tourBus.setNumThreads(*mNumThreads);
    }

    if (mMaxSequenceLength)
    {
        tourBus.setMaximumSequenceLength(*mMaxSequenceLength);
    }

    if (mMaxEditDistance)
    {
        tourBus.setMaximumEditDistance(*mMaxEditDistance);
    }

    if (mMaxRelativeErrors)
    {
        tourBus.setMaximumRelativeErrors(*mMaxRelativeErrors);
    }

    if (mCutoff)
    {
        tourBus.setCoverageCutoff(*mCutoff);
    }

    if (mRelCutoff)
    {
        tourBus.setCoverageRelativeCutoff(*mRelCutoff);
    }

    tourBus.pass();

    Graph::Builder b(g.K(), mOut, fac,
                     g.count() - tourBus.removedEdgesCount());


    log(info, "writing modified graph");
    tourBus.writeModifiedGraph(b);
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryPopBubbles::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    FileFactory& fac(pApp.fileFactory());
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    optional<uint64_t> numThreads;
    chk.getOptional("num-threads", numThreads);

    optional<uint64_t> cutoff;
    chk.getOptional("cutoff", cutoff);

    optional<double> relCutoff;
    chk.getOptional("relative-cutoff", relCutoff);

    optional<uint64_t> maxSequenceLength;
    chk.getOptional("max-sequence-length", maxSequenceLength);

    optional<uint64_t> maxEditDistance;
    chk.getOptional("max-edit-distance", maxEditDistance);

    optional<double> maxErrorRate;
    chk.getOptional("max-error-rate", maxErrorRate);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdPopBubbles(in, out, numThreads,
            maxSequenceLength, maxEditDistance, maxErrorRate,
            cutoff, relCutoff));
}


GossCmdFactoryPopBubbles::GossCmdFactoryPopBubbles()
    : GossCmdFactory("perform a bubble-popping pass over the graph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("cutoff");
    mCommonOptions.insert("relative-cutoff");
    mSpecificOptions.addOpt<uint64_t>("max-sequence-length", "",
            "maximum length of a sequence to consider");
    mSpecificOptions.addOpt<uint64_t>("max-edit-distance", "",
            "maximum edit distance to qualify as a bubble");
    mSpecificOptions.addOpt<double>("max-error-rate", "",
            "maximum error rate to qualify as a bubble");
}

