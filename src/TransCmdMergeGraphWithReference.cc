// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "TransCmdMergeGraphWithReference.hh"

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


class TransCmdMergeGraphWithReference : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    TransCmdMergeGraphWithReference(const std::string& pReference, const std::string& pIn, const std::string& pOut)
        : mReference(pReference), mIn(pIn), mOut(pOut)
    {
    }

private:

    const std::string mReference;
    const std::string mIn;
    const std::string mOut;
};


void
TransCmdMergeGraphWithReference::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    Graph::LazyIterator refitr(mReference, fac);
    Graph::LazyIterator itr(mIn, fac);

    // Check that the graphs are compatible.
    if (refitr.K() != itr.K())
    {
        string msg("graphs involved in a merge must have the same kmer-size.\n"
                     + mIn + " has k=" + lexical_cast<string>(itr.K()) + ".\n"
                     + mReference + " has k=" + lexical_cast<string>(refitr.K()) + ".\n");
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info(msg));
    }

    if (refitr.asymmetric() != itr.asymmetric())
    {
        string msg("graphs involved in a merge must either all preserve sense or not.\n"
                     + mIn + (itr.asymmetric() ? " preserves sense" : " does not preserve sense") + ".\n"
                     + mReference + (refitr.asymmetric() ? " preserves sense" : " does not preserve sense") + ".\n");
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info(msg));
    }

    log(info, "starting graph merge");

    Timer t;
    Graph::Builder dest(itr.K(), mOut, fac, itr.count(), itr.asymmetric());
    ProgressMonitorNew mon(log, itr.count());

    uint64_t work = 0;
    bool refitrValid = refitr.valid();
    while (itr.valid() && refitrValid)
    {
        std::pair<Graph::Edge,uint32_t> edge = *itr;
        std::pair<Graph::Edge,uint32_t> refedge = *refitr;

        while (refedge.first < edge.first)
        {
            ++refitr;
            refitrValid = refitr.valid();
            if (!refitrValid)
            {
                break;
            }
            refedge = *refitr;
        }

        if (refitrValid && refedge.first == edge.first)
        {
            dest.push_back(refedge.first.value(), refedge.second);
            ++refitr;
            refitrValid = refitr.valid();
        }
        ++itr;
        mon.tick(++work);
    }
    dest.end();

    log(info, "finishing graph merge");
    log(info, "total build time: " + lexical_cast<string>(t.check()));
}



GossCmdPtr
TransCmdFactoryMergeGraphWithReference::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileCreateCheck createChk(fac,true);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string ref;
    chk.getMandatory("graph-ref", ref);

    string out;
    chk.getMandatory("graph-out", out, createChk);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new TransCmdMergeGraphWithReference(ref, in, out));
}

TransCmdFactoryMergeGraphWithReference::TransCmdFactoryMergeGraphWithReference()
    : GossCmdFactory("create a new graph from an existing graph, using coverage information from a reference graph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");

    mSpecificOptions.addOpt<string>("graph-ref", "",
            "name of the reference graph object");
}


