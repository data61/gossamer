// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdPrintContigs.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "ProgressMonitor.hh"
#include "SuperGraph.hh"
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

    void printLinearSegments(FileFactory& pFac, Logger& pLog,
                             const string& pIn, const string& pOut, 
                             bool pOmitSequence, bool pVerboseHeaders, bool mNoLineBreaks,
                             bool pPrintRcs, uint64_t pL, uint64_t pC)
    {
        GraphPtr gPtr = Graph::open(pIn, pFac);
        Graph& g(*gPtr);
        if (g.asymmetric())
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info("Asymmetric graphs not yet handled")
                << Gossamer::open_graph_name_info(pIn));
        }

        FileFactory::OutHolderPtr outPtr(pFac.out(pOut));
        ostream& out(**outPtr);

        dynamic_bitset<> seen(g.count());

        vector<EdgeAndRank> edges;

        SmallBaseVector vec;

        if (pOmitSequence)
        {
            out << "Number\tLength\tMinCov\tMaxCov\tMeanCov\tStdDevCov" << endl;
        }

        const uint64_t cols = mNoLineBreaks ? -1 : 60;
        uint64_t conitNo = 1;
        ProgressMonitorNew mon(pLog, g.count());
        for (uint64_t i = 0; i < g.count(); ++i)
        {
            mon.tick(i);

            Graph::Edge e = g.select(i);
            Graph::Node e_f = g.from(e);
            if (g.inDegree(e_f) == 1 && g.outDegree(e_f) == 1)
            {
                continue;
            }

            if (seen[i])
            {
                continue;
            }

            Graph::Edge beg = e;

            edges.clear();
            Vis vis(edges);
            Graph::Edge end = g.linearPath(beg, vis);

            Graph::Edge end_rc = g.reverseComplement(end);
            uint64_t end_rc_rnk = g.rank(end_rc);
            BOOST_ASSERT(!seen[i]);
            BOOST_ASSERT(!seen[end_rc_rnk]);
            seen[i] = true;
            seen[end_rc_rnk] = true;

            uint64_t min_cov = numeric_limits<uint64_t>::max();

            for (uint64_t j = 0; j < edges.size(); ++j)
            {
                Graph::Edge x = edges[j].first;
                uint64_t x_rnk = edges[j].second;
                uint64_t x_cov = g.multiplicity(x_rnk);
                seen[x_rnk] = true;
                if (x_cov < min_cov)
                {
                    min_cov = x_cov;
                }

                if (!pPrintRcs) 
                {
                    Graph::Edge y = g.reverseComplement(x);
                    uint64_t y_rnk = g.rank(y);
                    BOOST_ASSERT(x_cov == g.multiplicity(y_rnk));
                    seen[y_rnk] = true;
                }
            }

            Graph::Node fst = g.from(edges.front().first);
            bool includeFst = (g.inDegree(fst) == 0 || g.canonical(fst));

            Graph::Node lst = g.to(edges.back().first);
            bool includeLst = (g.outDegree(lst) == 0 || g.antiCanonical(lst));

            uint64_t len = edges.size() + g.K();
            if (len >= g.K() && !includeFst)
            {
                len -= g.K();
            }
            if (len >= g.K() && !includeLst)
            {
                len -= g.K();
            }
            if (len >= pL && min_cov >= pC)
            {
                uint64_t s = 0;
                uint64_t s2 = 0;
                uint64_t n = edges.size();
                uint64_t minimum = numeric_limits<uint64_t>::max();
                uint64_t maximum = 0;
                for (uint64_t j = 0; j < n; ++j)
                {
                    uint64_t w = g.multiplicity(edges[j].second);
                    s += w;
                    s2 += w * w;
                    if (w > maximum)
                    {
                        maximum = w;
                    }
                    if (w < minimum)
                    {
                        minimum = w;
                    }
                }
                double a = static_cast<double>(s) / n;
                double d = sqrt(static_cast<double>(s2) / n - a * a);
                if (pOmitSequence)
                {
                    out << conitNo++ << '\t' << (n + g.K()) << '\t' << minimum << '\t' << maximum << '\t' << a << '\t' << d << endl;
                }
                else
                {
                    out << '>' << conitNo++;
                    if (pVerboseHeaders)
                    {
                        out << ' ' << (n + g.K()) << ':' << minimum << ':' << maximum << ':' << a << ':' << d;
                    }
                    out << endl;

                    vec.clear();
                    g.seq(edges[0].first, vec);
                    for (uint64_t j = 1; j < edges.size(); ++j)
                    {
                        vec.push_back(edges[j].first.value() & 3);
                    }
                    SmallBaseVector v(vec, (!includeFst) * g.K(), len);
                    v.print(out, cols);
                }
            }
        }
    }

} // namespace anonymous

void
GossCmdPrintContigs::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);
    Timer t;

    if (mPrintLinearSegments)
    {
        printLinearSegments(fac, log, mIn, mOut, mOmitSequence, mVerboseHeaders, mNoLineBreaks, mPrintRcs, mL, mC);
    }
    else
    {
        try 
        {
            auto sgPtr = SuperGraph::read(mIn, fac);
            FileFactory::OutHolderPtr outPtr(fac.out(mOut));
            ostream& out(**outPtr);
            sgPtr->printContigs(mIn, fac, log, out, mL, mOmitSequence, mVerboseHeaders, mNoLineBreaks, mPrintEntailed, mPrintRcs, mNumThreads);
        }
        catch (...)
        {
            printLinearSegments(fac, log, mIn, mOut, mOmitSequence, mVerboseHeaders, mNoLineBreaks, mPrintRcs, mL, mC);
        }
    }
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryPrintContigs::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());

    string in;
    chk.getRepeatingOnce("graph-in", in);

    uint64_t T = 1;
    chk.getOptional("num-threads", T);

    uint64_t c = 0;
    chk.getOptional("min-coverage", c);

    uint64_t l = 0;
    chk.getOptional("min-length", l);

    bool omitSequence = false;
    chk.getOptional("no-sequence", omitSequence);

    string out = "-";
    chk.getOptional("output-file", out, GossOptionChecker::FileCreateCheck(fac, false));

    bool printLinearSegs = false;
    chk.getOptional("print-linear-segments", printLinearSegs);

    bool verb = false;
    chk.getOptional("verbose-headers", verb);

    bool noBrk = false;
    chk.getOptional("no-line-breaks", noBrk);

    bool ent = false;
    chk.getOptional("print-entailed-contigs", ent);

    bool rcs = false;
    chk.getOptional("print-rcs", rcs);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdPrintContigs(in, c, l, rcs, omitSequence, printLinearSegs, verb, noBrk, ent, T, out));
}

GossCmdFactoryPrintContigs::GossCmdFactoryPrintContigs()
    : GossCmdFactory("print all the non-branching paths in the given assembly graph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("output-file");
    mCommonOptions.insert("no-sequence");
    mCommonOptions.insert("min-length");

    mSpecificOptions.addOpt<bool>("print-linear-segments", "",
            "Only print individual linear paths, even if a supergraph is present.");
    mSpecificOptions.addOpt<uint64_t>("min-coverage", "", "minimum coverage");
    mSpecificOptions.addOpt<bool>("verbose-headers", "", 
            "Print additional information in contig headers");
    mSpecificOptions.addOpt<bool>("no-line-breaks", "",
            "Print contig sequences without line breaks");
    mSpecificOptions.addOpt<bool>("include-entailed-contigs", "",
            "Print contigs even if they're entailed by others");
    mSpecificOptions.addOpt<bool>("print-rcs", "",
            "Include each sequence's reverse complement.");
}
