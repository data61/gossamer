// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdLintGraph.hh"

#include "Debug.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace // anonymous
{
    Debug abortOnError("abort-on-error", "abort when an error is detected during lint-graph");

    class Vis
    {
    public:
        void operator()(const Graph::Edge& pEdge)
        {
            mEdges.push_back(pEdge);
        }

        Vis(vector<Graph::Edge>& pEdges)
            : mEdges(pEdges)
        {
        }

    private:
        vector<Graph::Edge>& mEdges;
    };

    class GtZeroMsg
    {
    public:
        const string& operator()() const
        {
            if (mMsg.size() == 0)
            {
                mMsg = lexical_cast<string>(mVal) + " !> 0";
            }
            return mMsg;
        }

        GtZeroMsg(uint64_t pVal)
            : mVal(pVal)
        {
        }

    private:
        mutable string mMsg;
        uint64_t mVal;
    };

    template <typename T>
    void check_equal(Logger& pLog, const T& pLhs, const T& pRhs)
    {
        if (pLhs == pRhs)
        {
            return;
        }

        pLog(warning, lexical_cast<string>(pLhs) + " != " + lexical_cast<string>(pRhs));
        if (abortOnError.on())
        {
            abort();
        }
    }

    template <typename Msg>
    void check(Logger& pLog, bool pVal, const Msg& pMsg)
    {
        if (pVal)
        {
            return;
        }

        pLog(warning, pMsg());
        if (abortOnError.on())
        {
            abort();
        }
    }

    void seq(const SmallBaseVector& pVec, string& pStr)
    {
        for (uint64_t i = 0; i < pVec.size(); ++i)
        {
            static const char* map = "ACGT";
            pStr.push_back(map[pVec[i]]);
        }
    }
} // namespace anonymous


void
GossCmdLintGraph::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);

    bool asymmetric = g.asymmetric();

    if (mDumpProperties)
    {
       ostringstream out;
       g.stat().print(out, 1);
       istringstream in(out.str());
       log(info, "Graph properties:");
       string line;
       while (!in.eof())
       {
           line.clear();
           getline(in, line);
           log(info, line);
       }
    }

    log(info, "Pass 1: Checking counts are sane.");
    for (uint64_t i = 0; i < g.count(); ++i)
    {
        bool error = false;

        Graph::Edge e = g.select(i);
        uint32_t m = g.multiplicity(e);
        Graph::Edge e_p = g.reverseComplement(e);
        uint64_t r_p = 0;
        if (!g.accessAndRank(e_p, r_p))
        {
            SmallBaseVector fwdVec;
            g.seq(e, fwdVec);
            string fwd;
            seq(fwdVec, fwd);
            log(warning, "No reverse complement for the following edge exists:");
            log(warning, "  edge number " + lexical_cast<string>(i));
            log(warning, "  fwd edge    " + fwd + " " + lexical_cast<string>(m));
            continue;
        }

        if (asymmetric)
        {
            uint32_t m_p = g.multiplicity(r_p);
            if (m == 0 && m_p == 0)
            {
                SmallBaseVector fwdVec;
                g.seq(e, fwdVec);
                string fwd;
                seq(fwdVec, fwd);
                SmallBaseVector revVec;
                g.seq(e_p, revVec);
                string rev;
                seq(revVec, rev);
                log(warning, "neither fwd nor rev edge has a nonzero multiplicity:");
                log(warning, "  edge number " + lexical_cast<string>(i));
                log(warning, "  fwd edge    " + fwd + " " + lexical_cast<string>(m));
                log(warning, "  rev edge    " + rev + " " + lexical_cast<string>(m_p));
                error = true;
            }
        }
        else
        {
            uint32_t m_p = g.multiplicity(r_p);
            if(m != m_p)
            {
                SmallBaseVector fwdVec;
                g.seq(e, fwdVec);
                string fwd;
                seq(fwdVec, fwd);
                SmallBaseVector revVec;
                g.seq(e_p, revVec);
                string rev;
                seq(revVec, rev);
                log(warning, "counts on fwd and rev edges are not equal:");
                log(warning, "  edge number " + lexical_cast<string>(i));
                log(warning, "  fwd edge    " + fwd + " " + lexical_cast<string>(m));
                log(warning, "  rev edge    " + rev + " " + lexical_cast<string>(m_p));
                error = true;
            }
            check_equal(log, m, g.multiplicity(i));
            check(log, m > 0, GtZeroMsg(m));
        }
        if (error && abortOnError.on())
        {
            abort();
        }
    }

    log(info, "Pass 2: Checking traversal is sane.");
    uint64_t i = 0;
    for (Graph::Iterator itr(g); itr.valid(); ++itr, ++i)
    {
        Graph::Edge e = (*itr).first;

        Graph::Edge e2 = g.select(i);
        if (e != e2)
        {
            log(warning, "iterator and select conflict.");
            SmallBaseVector eVec;
            g.seq(e, eVec);
            string eStr;
            seq(eVec, eStr);
            SmallBaseVector e2Vec;
            g.seq(e2, e2Vec);
            string e2Str;
            seq(e2Vec, e2Str);
            log(warning, "    " + eStr);
            log(warning, "    " + e2Str);
            if (abortOnError.on())
            {
                abort();
            }
        }

        uint64_t r = g.rank(e);
        if (r != i)
        {
            log(warning, "iterator and rank conflict.");
            SmallBaseVector eVec;
            g.seq(e, eVec);
            string eStr;
            seq(eVec, eStr);
            log(warning, "iterator: " + lexical_cast<string>(i));
            log(warning, "rank    : " + lexical_cast<string>(r));
            if (abortOnError.on())
            {
                abort();
            }
        }
    }

}


GossCmdPtr
GossCmdFactoryLintGraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    chk.throwIfNecessary(pApp);

    bool dumpProperties;
    chk.getOptional("dump-properties", dumpProperties);

    return GossCmdPtr(new GossCmdLintGraph(in, dumpProperties));
}


GossCmdFactoryLintGraph::GossCmdFactoryLintGraph()
    : GossCmdFactory("verify that a graph structure is internally consistent")
{
    mCommonOptions.insert("graph-in");
    mSpecificOptions.addOpt<bool>("dump-properties", "", "show the internal properties of the graph");
}
