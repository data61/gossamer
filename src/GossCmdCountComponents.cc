// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdCountComponents.hh"

#include "LineSource.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "Logger.hh"
#include "ReadSequenceFileSequence.hh"
#include "ProgressMonitor.hh"
#include "ReverseComplementAdapter.hh"
#include "Timer.hh"

#include <list>
#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace {
    
Gossamer::rank_type rcRank(Graph& pG, Gossamer::rank_type pR)
{
    Graph::Edge e(pG.select(pR)); 
    e = pG.reverseComplement(e);
    return pG.rank(e);
}

struct CompInfo
{
    double mean() const
    {
        return mNumEdges ? mCountSum / double(mNumEdges) : 0;
    }

    double stdDev() const
    {
        return   mNumEdges 
               ? sqrt(double(mNumEdges) * mCountSum2 - (double(mCountSum) * mCountSum)) / mNumEdges
               : 0;
    }

    void add(uint64_t pCount)
    {
        mNumEdges += 1;
        mCountMin = min(mCountMin, pCount);
        mCountMax = max(mCountMax, pCount);
        mCountSum += pCount;
        mCountSum2 += pCount * pCount;
    }

    CompInfo(uint64_t pStart) :
        mStart(pStart),
        mNumEdges(0), mCountMin(numeric_limits<uint64_t>::max()), mCountMax(0), mCountSum(0), mCountSum2(0)
    {
    }

    uint64_t mStart;
    uint64_t mNumEdges;
    uint64_t mCountMin;
    uint64_t mCountMax;
    uint64_t mCountSum;
    uint64_t mCountSum2;
};

struct CompInfoGte
{
    bool operator()(const CompInfo& pC1 ,const CompInfo& pC2) const
    {
        return pC2.mNumEdges < pC1.mNumEdges;
    }
};

void follow(Graph& pG, Graph::Edge& pE, dynamic_bitset<>& pMarked, CompInfo& pInfo)
{
    vector<Graph::Node> ns;
    ns.push_back(pG.from(pE));

    while (!ns.empty())
    {
        Graph::Node n(ns.back());
        ns.pop_back();
        pair<Gossamer::rank_type, Gossamer::rank_type> p = pG.beginEndRank(n);

        // Trace forwards.
        for (Gossamer::rank_type r = p.first; r != p.second; ++r)
        {
            if (pMarked[r])
            {
                pMarked[r] = false;
                pInfo.add(pG.multiplicity(r));
                Graph::Edge e(pG.select(r));
                ns.push_back(pG.to(e));
            }
        }
    
        // Trace backwards.
        Graph::Node nRC(pG.reverseComplement(n));
        p = pG.beginEndRank(nRC);
        for (Gossamer::rank_type rRC = p.first; rRC != p.second; ++rRC)
        {
            uint64_t r = rcRank(pG, rRC);
            if (pMarked[r])
            {
                pMarked[r] = false;
                pInfo.add(pG.multiplicity(r));
                Graph::Edge e(pG.select(r));
                ns.push_back(pG.from(e));
            }
        }
    }
}

#if 0

void dive(Graph& pG, Graph::Node& pN, dynamic_bitset<>& pMarked, CompInfo& pInfo, bool pRC)
{
    vector<Graph::Node> ns;
    ns.push_back(pN);

    while (!ns.empty())
    {
        Graph::Node n(ns.back());
        ns.pop_back();
        pair<Gossamer::rank_type, Gossamer::rank_type> p = pG.beginEndRank(n);
        for (Gossamer::rank_type r = p.first; r != p.second; ++r)
        {
            Gossamer::rank_type r_fwd = pRC ? rcRank(pG, r) : r;
            if (pMarked[r_fwd])
            {
                pMarked[r_fwd] = false;
                pInfo.add(pG.multiplicity(r_fwd));
                Graph::Edge e(pG.select(r));
                ns.push_back(pG.to(e));
            }
        }
    }
}

void follow(Graph& pG, Graph::Edge& pE, dynamic_bitset<>& pMarked, CompInfo& pInfo)
{
    // Traverse forward from pE's target node.
    Graph::Node n_t(pG.to(pE));
    dive(pG, n_t, pMarked, pInfo, false);

    // Traverse backward from pE's source node.
    Graph::Node n_f(pG.from(pE));
    n_f = pG.reverseComplement(n_f);
    dive(pG, n_f, pMarked, pInfo, true);
}

#endif

};

void
GossCmdCountComponents::operator()(const GossCmdContext& pCxt)
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
    
    const uint64_t k = g.K();
    const uint64_t z = g.count();

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

    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    ReadSequenceFileSequence reads(items, fac, lineSrcFac, 0);

    dynamic_bitset<> marked(z);
    uint64_t numMarked = 0;
    uint64_t numEdges = 0;

    log(info, "marking used edges");
    if (items.empty())
    {
        marked.set();
        numMarked = z;
    }
    else
    {
        for (; reads.valid(); ++reads)
        {
            for (GossRead::Iterator i(*reads, k + 1); i.valid(); ++i, ++numEdges)
            {
                Graph::Edge e(i.kmer());
                Gossamer::rank_type r;
                if (g.accessAndRank(e, r))
                {
                    marked[r] = true;
                    numMarked += 1;
                }
            }
        }
    }

    log(info, "finding components");
    ProgressMonitorNew mon(log, z);
    vector<CompInfo> comps;
    for (uint64_t i = 0; i < z; ++i)
    {
        mon.tick(i);
        if (marked[i])
        {
            CompInfo info(i);
            info.add(g.multiplicity(i));
            Graph::Edge e(g.select(i));
            follow(g, e, marked, info);
            comps.push_back(info);
        }
    }

    cout << "Comp\tSize\tMin\tMax\tMean\tStd Dev\n";
    for (uint64_t i = 0; i < comps.size(); ++i)
    {
        const CompInfo& c(comps[i]);
        cout << i << '\t' << c.mNumEdges << '\t'
             << c.mCountMin << '\t' << c.mCountMax << '\t' 
             << c.mean() << '\t' << c.stdDev() << '\n';
    }


    if (!mOut.empty() && comps.size())
    {
        LOG(log, info) << "Writing largest component";
        for (uint64_t i = 0; i < 1; ++i)
        {
            const CompInfo& c(comps[i]);
            marked.set();
            CompInfo dummy(c.mNumEdges);
            Graph::Edge e(g.select(c.mStart));
            follow(g, e, marked, dummy);
    
            // Get rcs
            for (uint64_t i = 0; i < z; ++i)
            {
                if (!marked[i])
                {
                    Graph::Edge f(g.select(i));
                    uint64_t j = g.rank(g.reverseComplement(f));
                    marked[j] = false;
                }
            }
    

            uint64_t numEdges = 0;
            for (uint64_t j = 0; j < z; ++j)
            {
                numEdges += !marked[j] ? 1 : 0;
            }

            Graph::Builder b(g.K(), mOut /* + lexical_cast<string>(i) */, fac, numEdges);
            for (uint64_t i = 0; i < z; ++i)
            {
                if (!marked[i])
                {
                    b.push_back(g.select(i).value(), g.multiplicity(i));
                }
            }

            b.end();
        }
    }
}


GossCmdPtr
GossCmdFactoryCountComponents::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    chk.getOptional("graph-out", out);

    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    strings fastaNames;
    chk.getRepeating0("fasta-in", fastaNames, readChk);

    strings fastqNames;
    chk.getRepeating0("fastq-in", fastqNames, readChk);

    strings lineNames;
    chk.getRepeating0("line-in", lineNames, readChk);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdCountComponents(in, out, fastaNames, fastqNames, lineNames));
}

GossCmdFactoryCountComponents::GossCmdFactoryCountComponents()
    : GossCmdFactory("count connected components in the graph represented by the given reads")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
}
