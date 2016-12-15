// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdDotGraph.hh"

#include "LineSource.hh"
#include "EdgeCompiler.hh"
#include "ExternalSort64.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadHandler.hh"
#include "GossReadProcessor.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "ProgressMonitor.hh"
#include "ReadSequenceFileSequence.hh"
#include "ReverseComplementAdapter.hh"
#include "StringFileFactory.hh"
#include "Timer.hh"

#include <cmath>
#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;
typedef set<Graph::Edge> Edges;

namespace // anonymous
{
    
    uint8_t combineGroups(uint8_t pGroup1, uint8_t pGroup2)
    {
        if (pGroup1 == 0)
        {
            return pGroup2;
        }

        if (pGroup2 == 0)
        {
            return pGroup1;
        }

        if (pGroup1 == pGroup2)
        {
            return pGroup1;
        }

        return 255;
    }

    class RefReader : public GossReadHandler
    {
    public:
        void operator()(const GossRead& pRead)
        {
            checkSize();
            if (mCurGroup == 255)
            {
                // TODO: Warn!
                return;
            }
            for (GossRead::Iterator i(pRead, mGraph.K() + 1); i.valid(); ++i)
            {
                Graph::Edge e(i.kmer());
                uint64_t r;
                if (!mGraph.accessAndRank(e, r))
                {
                    continue;
                }
    
                mGroup[r] = combineGroups(mGroup[r], mCurGroup);
                Graph::Edge e_rc(mGraph.reverseComplement(e));
                bool hasRc = mGraph.accessAndRank(e_rc, r);
                (void)(&hasRc);
                BOOST_ASSERT(hasRc);
                mGroup[r] = combineGroups(mGroup[r], mCurGroup);
            }
            mCurGroup += 1;
        }
    
        void operator()(const GossRead& pLhs, const GossRead& pRhs)
        {
            BOOST_ASSERT(false);
        }

        RefReader(const Graph& pGraph, vector<uint8_t>& pGroup)
            : mGraph(pGraph), mGroup(pGroup), mCurGroup(1)
        {
        }

    private:
     
        void checkSize()
        {
            if (mGroup.empty())
            {
                mGroup.resize(mGraph.count(), 0);
            }
        }

        const Graph& mGraph;
        vector<uint8_t>& mGroup;
        uint8_t mCurGroup;
    };

    struct EdgeVisitor
    {
        const Graph::Edge& lastEdge() const
        {
            return mPrevEdge;
        }

        bool operator()(const Graph::Edge& pEdge, const uint64_t pRank)
        {
            if (!sameGroup(pRank))
            {
                return false;
            }

            mPrevEdge = pEdge;
            return true;
        }

        EdgeVisitor(const Graph::Edge& pFirstEdge, const uint64_t pFirstRank,
                    const vector<uint8_t>& pGroup)
            : mPrevEdge(pFirstEdge), mGroup(pGroup), 
              mCurGroup(pGroup.empty() ? 0 : pGroup[pFirstRank])
        {
        }

    private:

        bool sameGroup(uint64_t pRank) const
        {
            if (mGroup.empty())
            {
                return true;
            }
            
            return mCurGroup == mGroup[pRank] || mCurGroup == 255 || mGroup[pRank] == 255;
        }

        Graph::Edge mPrevEdge;
        const vector<uint8_t>& mGroup;
        const uint8_t mCurGroup;
    };

    struct CountMin
    {
        uint32_t value() const
        {
            return mValue;
        }

        void operator()(const CountMin& pOther)
        {
            mValue = min(mValue, pOther.mValue);
        }

        CountMin(uint32_t pValue)
            : mValue(pValue)
        {
        }

    private:
        uint32_t mValue;
    };

    struct CountMax
    {
        uint32_t value() const
        {
            return mValue;
        }

        void operator()(const CountMax& pOther)
        {
            mValue = max(mValue, pOther.mValue);
        }

        CountMax(uint32_t pValue)
            : mValue(pValue)
        {
        }

    private:
        uint32_t mValue;
    };

    struct CountMean
    {
        uint32_t value() const
        {
            return mMean;
        }

        void operator()(const CountMean& pOther)
        {
            uint64_t s = mSamples + pOther.mSamples;
            double tot = mMean * mSamples + pOther.mMean * pOther.mSamples;
            mMean = tot / s;
            mSamples = s;
        }

        CountMean(uint32_t pValue)
            : mMean(pValue), mSamples(1)
        {
        }

    private:
        double mMean;
        uint64_t mSamples;
    };

    template<typename CountMonoid>
    struct EdgeCollector
    {

        uint32_t count() const
        {
            return mCount.value();
        }

        bool operator()(const Graph::Edge& pEdge, const uint64_t pRank)
        {
            if (!sameGroup(pRank))
            {
                return false;
            }

            mSeen[pRank] = true;
            mVec.push_back(pEdge.value() & 3);
            mPrevEdge = pEdge;
            mCount(mGraph.weight(pRank));
            return true;
        }

        EdgeCollector(const Graph& pG,
                      const Graph::Edge& pFirstEdge, const uint64_t pFirstRank, 
                      SmallBaseVector& pVec, const vector<uint8_t>& pGroup, dynamic_bitset<>& pSeen)
            : mGraph(pG), mPrevEdge(pFirstEdge), mVec(pVec), mGroup(pGroup), mSeen(pSeen), 
              mCurGroup(pGroup.empty() ? 0 : pGroup[pFirstRank]),
              mCount(pG.weight(pFirstEdge))
        {
            mVec.clear();
            pG.seq(pG.from(pFirstEdge), mVec);
        }

    private:
        
        bool sameGroup(uint64_t pRank) const
        {
            if (mGroup.empty())
            {
                return true;
            }
            
            return mCurGroup == mGroup[pRank] || mCurGroup == 255 || mGroup[pRank] == 255;
        }

        const Graph& mGraph;
        Graph::Edge mPrevEdge;
        SmallBaseVector& mVec;
        const vector<uint8_t>& mGroup;
        dynamic_bitset<>& mSeen;
        const uint8_t mCurGroup;
        CountMonoid mCount;
    };

    void linearPath(const Graph& pG, const vector<uint8_t>& pGroup,
                    const Graph::Edge& pE, dynamic_bitset<>& pSeen,
                    SmallBaseVector& pV, uint32_t& pC)
    {
        // Traverse back to the start node.
        Graph::Edge e_rc(pG.reverseComplement(pE));
        uint64_t r = pG.rank(e_rc);
        EdgeVisitor vis(e_rc, r, pGroup);
        pG.linearPath(e_rc, vis);

        Graph::Edge start_rc = vis.lastEdge();
        Graph::Edge start = pG.reverseComplement(start_rc);
        r = pG.rank(start);

        // Now forward to the end node.
        EdgeCollector<CountMean> col(pG, start, r, pV, pGroup, pSeen);
        pG.linearPath(start, col);
        pC = col.count();
    }

    void getLabels(const SmallBaseVector& pV, uint64_t pK, 
                   string& pPre, string& pSuf, string& pWhole)
    {
        static const char base[4] = {'A','C','G','T'};
        const uint64_t sz = pV.size();
        stringstream pre, suf, whole;
        BOOST_ASSERT(pK <= sz);

        for (uint64_t i = 0; i < sz; ++i)
        {
            const char b = base[pV[i]];
            whole << b;
            if (i < pK)
            {
                pre << b;
            }
            if (i >= sz - pK)
            {
                suf << b;
            }
        }

        pPre = pre.str();
        pSuf = suf.str();
        pWhole = whole.str();
    }

    void writeNode(ostream& out, const GossCmdDotGraph::Style& pStyle, 
                   const string& nodeL)
    {
        const string colour("black");
        const string& shape(pStyle.mPointNodes ? "point" : "box");
        out << '\t' << nodeL 
            << " ["
                << "shape=\"" << shape << "\", "
                << "label=\"" << (pStyle.mLabelNodes ? " " + nodeL : "") << "\" "
                << "fontcolor=\"" << colour << "\""
                << "color=\"" << colour << "\""
            << "];" << endl;
    }

    void writeEdge(ostream& out, const GossCmdDotGraph::Style& pStyle, 
                   const string& fromL, const string& toL,
                   const string& edgeL, uint64_t c, const uint64_t pGroup)
    {
        stringstream ss;
        if (pStyle.mLabelEdges)
        {
            ss << " ";
            ss << edgeL;
            if (pStyle.mShowCounts)
            {
                ss << ",";
            }
        }
        if (pStyle.mShowCounts)
        {
            ss << " ";
            ss << c;
        }
        string l = ss.str();

        const string& colour(pStyle.mColours[pGroup]);
        out << '\t' << fromL << " -> " << toL
            << " ["
                << "style=\"setlinewidth(" << pStyle.scaleEdge(c) << ")\" "
                << "label=\"" << l << "\" "
                << "weight=\"" << c << "\" "
                << "fontcolor=\"" << colour << "\""
                << "color=\"" << colour << "\""
            << "];" << endl;
    }

}

void
GossCmdDotGraph::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    GraphPtr gPtr = Graph::open(mIn, fac);
    const Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }
    const uint64_t K(g.K());

    vector<uint8_t> group;
    RefReader rdr(g, group);
    GossReadProcessor::processSingle(pCxt, mFastaNames, mFastqNames, mLineNames, rdr);

    FileFactory::OutHolderPtr outPtr(fac.out(mOut));
    ostream& out(**outPtr);

    ProgressMonitorNew mon(log, g.count());
    dynamic_bitset<> seen(g.count());
    SmallBaseVector v;
    string fromL, toL, edgeL;
    out << "digraph G {" << endl;
    uint64_t r = 0;
    uint8_t grp = 255;
    for (Graph::Iterator itr(g); itr.valid(); ++itr, ++r)
    {
        mon.tick(r);
        Graph::Edge e = (*itr).first;
        if (!seen[r])
        {
            seen[r] = true;
            v.clear();
            uint32_t c;
            if (!group.empty())
            {
                grp = group[r];
            }

            if (mStyle.mCollapseLinearPaths)
            {
                linearPath(g, group, e, seen, v, c);
            }
            else
            {
                c = (*itr).second;
                g.seq(e, v);
            }

            Graph::Node from(v.kmer(K, 0));
            Graph::Node to(v.kmer(K, v.size() - K));
            if (   mSkipSingletons
                && g.inDegree(from) == 0
                && g.outDegree(to) == 0)
            {
                continue;
            }

            getLabels(v, K, fromL, toL, edgeL);

            writeNode(out, mStyle, fromL);
            writeEdge(out, mStyle, fromL, toL, edgeL, c, grp);
            if (g.outDegree(to) == 0)
            {
                writeNode(out, mStyle, toL);
            }
        }
    }
    out << '}' << endl;
}


GossCmdPtr
GossCmdFactoryDotGraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    strings fastaNames;
    chk.getRepeating0("fasta-in", fastaNames, readChk);

    strings fastqNames;
    chk.getRepeating0("fastq-in", fastqNames, readChk);

    strings lineNames;
    chk.getRepeating0("line-in", lineNames, readChk);

    string out = "-";
    chk.getOptional("output-file", out, GossOptionChecker::FileCreateCheck(fac, false));

    bool skipSings = false;
    chk.getOptional("skip-singletons", skipSings);

    bool oneColour = false;
    chk.getOptional("one-colour", oneColour);

    GossCmdDotGraph::Style style;
    chk.getOptional("label-nodes", style.mLabelNodes);
    chk.getOptional("label-edges", style.mLabelEdges);
    chk.getOptional("show-counts", style.mShowCounts);
    chk.getOptional("collapse-linear-paths", style.mCollapseLinearPaths);
    chk.getOptional("point-nodes", style.mPointNodes);

    if (oneColour)
    {
        style.mColours.resize(256, "black");
        style.mColours[0] = "grey70";
    }
    else
    {
        style.mColours.resize(256, "crimson");
        style.mColours[0] = "grey70";
        style.mColours[255] = "black";
        style.mColours[1] = "blue";
        style.mColours[2] = "red";
        style.mColours[3] = "green";
        style.mColours[4] = "yellow";
    }


    bool scale_const = false;
    bool scale_linear = false;
    bool scale_sqrt = false;
    bool scale_log = false;
    chk.getOptional("edge-scale-const", scale_const);
    chk.getOptional("edge-scale-linear", scale_linear);
    chk.getOptional("edge-scale-sqrt", scale_sqrt);
    chk.getOptional("edge-scale-log", scale_log);

    chk.throwIfNecessary(pApp);

    if ((int(scale_const) + int(scale_linear) + int(scale_sqrt) + int(scale_log)) > 1)
    {
        BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::usage_info("multiple edge scales requested"));
    }
    style.mEdgeScale =   scale_const ? GossCmdDotGraph::Style::Const
                       : scale_linear ? GossCmdDotGraph::Style::Linear
                       : scale_sqrt ? GossCmdDotGraph::Style::Sqrt
                       : GossCmdDotGraph::Style::Log;

    return GossCmdPtr(new GossCmdDotGraph(in, out, fastaNames, fastqNames, lineNames, skipSings, style));
}

GossCmdFactoryDotGraph::GossCmdFactoryDotGraph()
    : GossCmdFactory("write out the graph in dot format.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("output-file");

    mSpecificOptions.addOpt<bool>("label-nodes", "",
            "label nodes");
    mSpecificOptions.addOpt<bool>("label-edges", "",
            "label edges");
    mSpecificOptions.addOpt<bool>("show-counts", "",
            "display edge counts");
    mSpecificOptions.addOpt<bool>("collapse-linear-paths", "",
            "display linear paths as single edges");
    mSpecificOptions.addOpt<bool>("point-nodes", "",
            "draw nodes as points (labels omitted)");
    mSpecificOptions.addOpt<bool>("edge-scale-const", "",
            "draw all edges with the same width");
    mSpecificOptions.addOpt<bool>("edge-scale-linear", "",
            "use a linear scale for edge widths");
    mSpecificOptions.addOpt<bool>("edge-scale-sqrt", "",
            "use a square root scale for edge widths");
    mSpecificOptions.addOpt<bool>("edge-scale-log", "",
            "use a log scale for edge widths (the default)");
    mSpecificOptions.addOpt<bool>("skip-singletons", "",
            "ignore edges (or linear paths) without coincident edges");
    mSpecificOptions.addOpt<bool>("one-colour", "",
            "use the same colour for all highlighted edges");
}
