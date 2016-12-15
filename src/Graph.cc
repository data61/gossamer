// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Graph.hh"
#include "GossamerException.hh"

#include "Debug.hh"

using namespace Gossamer;
using namespace boost;
using namespace std;

namespace
{

    class CountAccumulator
    {
    public:
        void operator()(const Graph::Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            mValue += mCounts[pRank];
        }

        uint64_t value() const
        {
            return mValue;
        }

        CountAccumulator(const Graph& pGraph, const VariableByteArray& pCounts)
            : mGraph(pGraph), mCounts(pCounts), mValue(0)
        {
        }

    private:
        const Graph& mGraph;
        const VariableByteArray& mCounts;
        uint64_t mValue;
    };

    Debug dumpOnOpen("dump-graph-on-open", "Dump the edges of the graph on opening");

    class BitmapRemover
    {
    public:
        bool valid() const
        {
            return mCurr < mBitmap.size();
        }

        uint64_t operator*() const
        {
            BOOST_ASSERT(mBitmap[mCurr]);
            return mCurr;
        }

        void operator++()
        {
            ++mCurr;
            if (valid())
            {
                scan();
            }
        }

        BitmapRemover(const dynamic_bitset<>& pBitmap)
            : mBitmap(pBitmap), mCurr(0)
        {
            scan();
        }

    private:
        void scan()
        {
            while (mCurr < mBitmap.size() && !mBitmap[mCurr])
            {
                ++mCurr;
            }
        }

        const dynamic_bitset<>& mBitmap;
        uint64_t mCurr;
    };

    void
    getAndVerifyHeader(const string& pBaseName, FileFactory& pFactory, Graph::Header& pHeader)
    {
        FileFactory::InHolderPtr ip(pFactory.in(pBaseName + ".header"));
        istream& i(**ip);
        uint64_t version;

        istream::pos_type start = i.tellg();
        i.read(reinterpret_cast<char*>(&version), sizeof(version));
        i.seekg(start);

        if (version == Graph::version)
        {
            istream& i(**ip);
            i.read(reinterpret_cast<char*>(&pHeader), sizeof(pHeader));
        }
        else
        {
            uint64_t v = Graph::version;
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::version_mismatch_info(pair<uint64_t,uint64_t>(version, v)));
        }
    }
}

void
Graph::Builder::end()
{
    uint64_t Rho = mK + 1;
    mEdgesBuilderBackground.end();
    mCountsBuilderBackground.end();
    mEdgesBuilderBackground.wait();
    mEdgesBuilder.end((position_type(1) << (2 * Rho)));

    mCountsBuilderBackground.wait();
    mCountsBuilder.end();

    FileFactory::OutHolderPtr op(mFactory.out(mBaseName + "-counts-hist.txt"));
    ostream& o(**op);
    for (map<uint64_t,uint64_t>::const_iterator i = mHist.begin();
            i != mHist.end(); ++i)
    {
        o << i->first << '\t' << i->second << endl;
    }
}

PropertyTree
Graph::Builder::stat() const
{
    PropertyTree t;
    t.putSub("background-edge-builder", mEdgesBuilderBackground.stat());
    t.putSub("background-counts-builder", mCountsBuilderBackground.stat());
    return t;
}

Graph::Builder::Builder(uint64_t pK, const string& pBaseName, FileFactory& pFactory, rank_type pNumEdges, bool pAsymmetric)
    : mBaseName(pBaseName), mFactory(pFactory), mK(pK),
      mEdgesBuilder(pBaseName + "-edges", pFactory, position_type(1) << (2 * pK + 2), pNumEdges),
      mEdgesBuilderBackground(mEdgesBuilder, 4096, 1024),
      mCountsBuilder(pBaseName + "-counts", pFactory, pNumEdges, 1.0 / 1024.0),
      mCountsBuilderBackground(mCountsBuilder, 4096, 1024)
{
    if (pK > MaxK)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("unable to build a graph with k="
                                                    + lexical_cast<string>(pK)));
    }
    Header h;
    h.version = version;
    h.K = pK;
    h.flags[Header::fAsymmetric] = pAsymmetric;

    FileFactory::OutHolderPtr op(pFactory.out(pBaseName + ".header"));
    ostream& o(**op);
    o.write(reinterpret_cast<const char*>(&h), sizeof(h));
}


Graph::Builder::Builder(uint64_t pK, const string& pBaseName, FileFactory& pFactory, D pD, bool pAsymmetric)
    : mBaseName(pBaseName), mFactory(pFactory), mK(pK),
      mEdgesBuilder(pBaseName + "-edges", pFactory, pD.value()),
      mEdgesBuilderBackground(mEdgesBuilder, 4096, 1024),
      mCountsBuilder(pBaseName + "-counts", pFactory, 1024ULL * 1024ULL * 1024ULL, 1.0 / 1024.0),
      mCountsBuilderBackground(mCountsBuilder, 4096, 1024)
{
    if (pK > MaxK)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("unable to build a graph with k="
                                                    + lexical_cast<string>(pK)));
    }

    Header h;
    h.version = version;
    h.K = pK;
    h.flags[Header::fAsymmetric] = pAsymmetric;

    FileFactory::OutHolderPtr op(pFactory.out(pBaseName + ".header"));
    ostream& o(**op);
    o.write(reinterpret_cast<const char*>(&h), sizeof(h));
}

Graph::LazyIterator::LazyIterator(const string& pBaseName, FileFactory& pFactory)
    : mCount(0),
      mEdgesItr(SparseArray::lazyIterator(pBaseName + "-edges", pFactory)),
      mCountsItr(VariableByteArray::lazyIterator(pBaseName + "-counts", pFactory))
{
    getAndVerifyHeader(pBaseName, pFactory, mHeader);

    // Parse X-counts-hist.txt to determine count.
    FileFactory::InHolderPtr ip(pFactory.in(pBaseName + "-counts-hist.txt"));
    istream& in(**ip);
    uint64_t m, c;
    do
    {
        in >> m >> c;
        if (!in.good())
        {
            break;
        }
        mCount += c;
    }
    while (true);
}

// Return true if there is no edge proceeding
// from the same node as this edge proceeds from
// that has a greater multiplicity.
//
bool
Graph::majorityEdge(const Edge& pEdge) const
{
    Node n = from(pEdge);
    pair<rank_type,rank_type> rx = beginEndRank(n);
    rank_type r0 = rx.first;
    rank_type r1 = rx.second;
    rank_type r = rank(pEdge);
    uint64_t m = multiplicity(r);
    for (rank_type en = r0; en < r1; ++en)
    {
        if (en == r)
        {
            continue;
        }
        if (multiplicity(r) > m)
        {
            return false;
        }
    }
    return true;
}

// Find the lowest numbered edge such that no
// other edge proceeding from the same node has
// greater multiplicity.
//
Graph::Edge
Graph::majorityEdge(const Node& pNode) const
{
    pair<rank_type,rank_type> rx = beginEndRank(pNode);
    rank_type r0 = rx.first;
    rank_type r1 = rx.second;
    rank_type r = r0;
    uint64_t m = multiplicity(r0);
    for (rank_type en = r0 + 1; en < r1; ++en)
    {
        if (multiplicity(en) > m)
        {
            r = en;
            m = multiplicity(en);
        }
    }
    return select(r);
}


// Compute the sum of the multiplicities for all the edges
// in the given linear segment.
//
uint64_t
Graph::weight(const Edge& pBegin, const Edge& pEnd) const
{
    CountAccumulator acc(*this, mCounts);
    visitPath(pBegin, pEnd, acc);
    return acc.value();
}

void
Graph::tracePath1(const Edge& pBegin, const Edge& pEnd, SmallBaseVector& pSeq) const
{
    Edge e = pBegin;
    Node n = to(e);
    while (e != pEnd)
    {
        pSeq.push_back(e.value() & 3);
        Node n = to(e);
        e = onlyOutEdge(n);
    }
    pSeq.push_back(e.value() & 3);
}

void
Graph::tracePath(const Edge& pBegin, const Edge& pEnd, vector<Edge>& pEdges) const
{
    Edge e = pBegin;
    Node n = to(e);
    pEdges.push_back(e);
    while (inDegree(n) == 1 && outDegree(n) == 1)
    {
        e = onlyOutEdge(n);
        n = to(e);
        pEdges.push_back(e);
    }
}

void
Graph::tracePath(const Edge& pBegin, const Edge& pEnd, vector<Node>& pNodes) const
{
    Edge e = pBegin;
    Node n = to(e);
    pNodes.push_back(from(e));
    pNodes.push_back(n);
    while (inDegree(n) == 1 && outDegree(n) == 1)
    {
        e = onlyOutEdge(n);
        n = to(e);
        pNodes.push_back(n);
    }
}

void
Graph::traceMajorityPath(const Node& pBegin, uint64_t pLength, SmallBaseVector& pSeq) const
{
    Node n = pBegin;
    seq(n, pSeq);
    while (outDegree(n) > 0 && pSeq.size() < pLength)
    {
        Edge e = majorityEdge(n);
        pSeq.push_back(e.value() & 3);
        n = to(e);
    }
}


void
Graph::remove(const dynamic_bitset<>& pBitmap)
{
    BitmapRemover r(pBitmap);
    mEdgesView.remove(r);
}


map<uint64_t,uint64_t>
Graph::hist(const string& pBaseName, FileFactory& pFactory)
{
    FileFactory::InHolderPtr inp(pFactory.in(pBaseName + "-counts-hist.txt"));
    istream& in(**inp);

    map<uint64_t,uint64_t> r;
    while (in.good())
    {
        uint64_t c;
        uint64_t f;
        in >> c >> f;
        if (!in.good())
        {
            break;
        }
        r[c] = f;
    }
    return r;
}

GraphPtr
Graph::open(const string& pBaseName, FileFactory& pFactory)
{
    try
    {
        return GraphPtr(new Graph(pBaseName, pFactory));
    }
    catch (Gossamer::error& exc)
    {
        exc << Gossamer::open_graph_name_info(pBaseName);
        throw;
    }
}

void
Graph::remove(const string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".header");
    pFactory.remove(pBaseName + "-counts-hist.txt");
    SparseArray::remove(pBaseName + "-edges", pFactory);
    VariableByteArray::remove(pBaseName + "-counts", pFactory);
}

Graph::Graph(const string& pBaseName, FileFactory& pFactory)
    : mEdges(pBaseName + "-edges", pFactory),
      mEdgesView(mEdges),
      mCounts(pBaseName + "-counts", pFactory)
{
    getAndVerifyHeader(pBaseName, pFactory, mHeader);
    mM = (position_type(1) << (2 * K())) - 1;

    if (dumpOnOpen.on())
    {
        cerr << "dumping " << pBaseName << endl;
        SmallBaseVector v;
        for (rank_type i = 0; i < count(); ++i)
        {
            Edge e = select(i);
            v.clear();
            seq(e, v);
            cerr << v << endl;
        }
        cerr << "." << endl;
    }
}

