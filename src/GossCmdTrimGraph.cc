// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdTrimGraph.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "EstimateGraphStatistics.hh"
#include "ProgressMonitor.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;


void
GossCmdTrimGraph::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    log(info, "scanning histogram");
    uint64_t cutoff = mC;
    uint64_t z = 0;
    uint64_t n = 0;
    uint64_t k = 0;
    Timer t;
    {
        Graph::LazyIterator itr(mIn, fac);
        if (itr.asymmetric())
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info("Asymmetric graphs not yet handled")
                << Gossamer::open_graph_name_info(mIn));
        }
        k = itr.K();
        z = itr.count();
        map<uint64_t,uint64_t> h = Graph::hist(mIn, fac);

        if (mScaleCutoffByK)
        {
            uint64_t scaleK = *mScaleCutoffByK;
            if (scaleK < k)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::usage_info("Scale k-mer size is greater than graph k-mer size"));
            }

            BOOST_ASSERT(k > 0);

            cutoff = cutoff * scaleK / k;
            log(info, "  using scalled cutoff " + lexical_cast<string>(cutoff));
        }

        if (mInferCutoff)
        {
            EstimateGraphStatistics estimate(log, h);
            estimate.report(k);
            if (estimate.modelFits())
            {
                cutoff = estimate.estimateTrimPoint();
                if (mEstimateOnly)
                {
                    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
                    return;
                }
            }
            else
            {
                log(warning, "Could not estimate cutoff.");
                if (mEstimateOnly)
                {
                    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
                    return;
                }
                log(warning, "Defaulting to cutoff = 1");
                cutoff = 1;
            }
            if (cutoff == 0)
            {
                log(warning, "Cutoff = 0 estimated; defaulting to cutoff = 1");
                cutoff = 1;
            }
        }
        for (map<uint64_t,uint64_t>::const_iterator i = h.begin();
                i != h.end(); ++i)
        {
            if (i->first > cutoff)
            {
                n += i->second;
            }
        }
    }

    log(info, "scanning to trim edges");
    log(info, mIn + " had " + lexical_cast<string>(z));
    log(info, mOut + " will have " + lexical_cast<string>(n));

    Graph::Builder b(k, mOut, fac, n);

    ProgressMonitorNew mon(log, z);
    uint64_t j = 0;

    for (Graph::LazyIterator itr(mIn, fac); itr.valid(); ++itr)
    {
        mon.tick(++j);
        if ((*itr).second > cutoff)
        {
            b.push_back((*itr).first.value(), (*itr).second);
        }
    }
    b.end();

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryTrimGraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    FileFactory& fac(pApp.fileFactory());
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    bool inferCutoff = false;
    uint64_t c = 0;
    if (!chk.getOptional("cutoff", c))
    {
        inferCutoff = true;
    }

    optional<uint64_t> scaleCutoffByK;
    chk.getOptional("scale-cutoff-by-k", scaleCutoffByK);

    bool estimateOnly = false;
    chk.getOptional("estimate-only", estimateOnly);

    if (estimateOnly && !inferCutoff)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::usage_info("cannot estimate cutoff unless it is also being inferred"));
    }

    if (inferCutoff && scaleCutoffByK)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::usage_info("cannot scale an inferred cutoff"));
    }

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdTrimGraph(in, out, c, inferCutoff, estimateOnly, scaleCutoffByK));
}

GossCmdFactoryTrimGraph::GossCmdFactoryTrimGraph()
    : GossCmdFactory("create a new graph by trimming low frequency edges")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("cutoff");
    mCommonOptions.insert("estimate-only");
    mSpecificOptions.addOpt<uint64_t>("scale-cutoff-by-k", "",
            "scale the coverage cutoff by k-mer size");
}

