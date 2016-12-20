// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "ScaffoldGraph.hh"

using namespace std;
using namespace boost;

constexpr uint64_t ScaffoldGraph::version;

namespace // anonymous
{
    class LinkReader
    {
    public:

        struct Link
        {
            Link()
                : mLhs(0), mRhs(0), mCount(0), mGap(0), mGapRange(0)
            {
            }

            SuperPathId mLhs;
            SuperPathId mRhs;
            uint64_t mCount;
            int64_t mGap;
            int64_t mGapRange;
        };

        struct LinkGte
        {
            const bool operator()(const Link& pX, const Link& pY) const
            {
                if (pX.mLhs > pY.mLhs)
                {
                    return true;
                }
                else if (pX.mLhs == pY.mLhs)
                {
                    if (pX.mRhs >= pY.mRhs)
                    {
                        return true;
                    }

                    return false;
                }
                
                return false;
            }
        };

        struct LinkLt
        {
            const bool operator()(const Link& pX, const Link& pY) const
            {
                LinkGte gte;
                return !gte(pX, pY);
            }
        };

        struct LinkEq
        {
            const bool operator()(const Link& pX, const Link& pY) const
            {
                return pX.mLhs == pY.mLhs && pX.mRhs == pY.mRhs;
            }
        };
        
        bool valid() const
        {
            return mValid;
        }

        void operator++()
        {
            next();
        }

        const Link& operator*() const
        {
            return mCur;
        }

        LinkReader(const string& pBaseName, FileFactory& pFactory)
            : mInPtr(pFactory.in(pBaseName + ".links")), mIn(**mInPtr), mCur(), mValid(true)
        {
            FileFactory::InHolderPtr ip;
            try
            {
                ip = pFactory.in(pBaseName + ".header");
            }
            catch (...)
            {
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << Gossamer::general_error_info("No scaffold graph header file found."));
            }
            ScaffoldGraph::Header h;
            (**ip).read(reinterpret_cast<char*>(&h), sizeof(h));
            if (h.version != ScaffoldGraph::version)
            {
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << boost::errinfo_file_name(pBaseName + ".header")
                        << Gossamer::version_mismatch_info(make_pair(ScaffoldGraph::version, h.version)));
            }
            mCur.mGapRange = h.insertRange;

            next();
        }

    private:

        void next()
        {
            while (mIn.good())
            {
                char k;
                mIn.get(k);
                if (!isspace((unsigned char)k))
                {
                    mIn.unget();
                    break;
                }
            }
            if (!mIn.good())
            {
                mValid = false;
                return;
            }

            uint64_t l;
            uint64_t r;
            uint64_t c;
            int64_t g;
            mIn >> l >> r >> c >> g;
            if (mIn.fail() || mIn.bad())
            {
                throw "failed to read from scaffold file!";
            }
            mCur.mLhs = SuperPathId(l);
            mCur.mRhs = SuperPathId(r);
            mCur.mGap = g;
            mCur.mCount = c;
        }

        FileFactory::InHolderPtr mInPtr;
        istream& mIn;
        Link mCur;
        bool mValid;
    };
    typedef std::shared_ptr<LinkReader> LinkReaderPtr;

}
// namespace anonymous

void ScaffoldGraph::Builder::push_back(
        const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
        const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
        const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2)
{
        if (!pCount)
        {
            return;
        }
        const int64_t lOfs = pLhsOffsetSum / int64_t(pCount);
        const int64_t rOfs = pRhsOffsetSum / int64_t(pCount);
        const int64_t len = mSuperGraph.baseSize(pLhs) - lOfs + rOfs;
        const int64_t gap = int64_t(mHeader.insertSize) - len;

        mScafFile << pLhs.value() << '\t' << pRhs.value() << '\t' << pCount << '\t' << gap << '\n';
}

void ScaffoldGraph::Builder::end()
{
    FileFactory::OutHolderPtr op(mFactory.out(mBaseName + ".header"));
    ostream& o(**op);
    o.write(reinterpret_cast<const char*>(&mHeader), sizeof(mHeader));
}

ScaffoldGraph::Builder::Builder(const std::string& pBaseName, FileFactory& pFactory, 
                                const SuperGraph& pSuperGraph,
                                const uint64_t pInsertSize, const uint64_t pInsertRange, 
                                const Orientation pOrientation)
    : mBaseName(pBaseName), mFactory(pFactory), mSuperGraph(pSuperGraph),
      mHeader(pInsertSize, pInsertRange, pOrientation),
      mScafFilePtr(pFactory.out(pBaseName + ".links")),
      mScafFile(**mScafFilePtr)
{
}

void ScaffoldGraph::dumpNodes(ostream& pOut) const
{
    pOut << "dumpNodes\n";
    for (NodeMap::const_iterator i = mNodes.begin(); i != mNodes.end(); ++i)
    {
        pOut << i->first.value() << '\n';
    }
    pOut << "--------------\n";
}

void ScaffoldGraph::dumpDot(ostream& pOut, const SuperGraph& pSg) const
{
    for (NodeMap::const_iterator i = mNodes.begin(); i != mNodes.end(); ++i)
    {
        const SuperPathId a(i->first);
        uint64_t sz(pSg.baseSize(a));
        pOut << a.value() << " [label = \"" << a.value() << " (" << sz << ")\"];\n";
        for (Edges::const_iterator j = i->second.mTo.begin(); j != i->second.mTo.end(); ++j)
        {
            const SuperPathId b(j->get<0>());
            pOut << a.value() << " -> " << b.value() << " [label = \"" << j->get<1>() 
                 << " (" << j->get<2>() << ")\"];\n";
        }
    }
}

void ScaffoldGraph::getNodes(vector<SuperPathId>& pNodes) const
{
    pNodes.clear();
    for (NodeMap::const_iterator i = mNodes.begin(); i != mNodes.end(); ++i)
    {
        pNodes.push_back(i->first);
    }
}

void ScaffoldGraph::getNodes(unordered_set<SuperPathId>& pNodes) const
{
    pNodes.clear();
    for (NodeMap::const_iterator i = mNodes.begin(); i != mNodes.end(); ++i)
    {
        pNodes.insert(i->first);
    }
}

void ScaffoldGraph::getNodesAndRcs(const SuperGraph& pSg, unordered_set<SuperPathId>& pNodes) const
{
    pNodes.clear();
    for (NodeMap::const_iterator i = mNodes.begin(); i != mNodes.end(); ++i)
    {
        pNodes.insert(i->first);
        pNodes.insert(pSg.reverseComplement(i->first));
    }
}

// Collects the nodes reachable via forward and backward edges from pNode.
void ScaffoldGraph::getConnectedNodes(SuperPathId pNode, unordered_set<SuperPathId>& pNodes) const
{
    pNodes.clear();
    vector<SuperPathId> stack(1, pNode);
    while (!stack.empty())
    {
        SuperPathId n(stack.back());
        stack.pop_back();
        if (pNodes.count(n))
        {
            continue;
        }
        pNodes.insert(n);
        const ScaffoldGraph::Edges& froms(getFroms(n));
        for (ScaffoldGraph::Edges::const_iterator i = froms.begin(); i != froms.end(); ++i)
        {
            stack.push_back(i->get<0>());
        }
        const ScaffoldGraph::Edges& tos(getTos(n));
        for (ScaffoldGraph::Edges::const_iterator i = tos.begin(); i != tos.end(); ++i)
        {
            stack.push_back(i->get<0>());
        }
    }
}

// Collects the nodes reachable via forward edges from pNode.
void ScaffoldGraph::getNodesFrom(SuperPathId pNode, unordered_set<SuperPathId>& pNodes) const
{
    pNodes.clear();
    vector<SuperPathId> stack(1, pNode);
    while (!stack.empty())
    {
        SuperPathId n(stack.back());
        stack.pop_back();
        if (pNodes.count(n))
        {
            continue;
        }
        pNodes.insert(n);
        const ScaffoldGraph::Edges& tos(getTos(n));
        for (ScaffoldGraph::Edges::const_iterator i = tos.begin(); i != tos.end(); ++i)
        {
            stack.push_back(i->get<0>());
        }
    }
}

// Collects the nodes which can reach pNode via forward edges.
void ScaffoldGraph::getNodesTo(SuperPathId pNode, unordered_set<SuperPathId>& pNodes) const
{
    pNodes.clear();
    vector<SuperPathId> stack(1, pNode);
    while (!stack.empty())
    {
        SuperPathId n(stack.back());
        stack.pop_back();
        if (pNodes.count(n))
        {
            continue;
        }
        pNodes.insert(n);
        const ScaffoldGraph::Edges& froms(getFroms(n));
        for (ScaffoldGraph::Edges::const_iterator i = froms.begin(); i != froms.end(); ++i)
        {
            stack.push_back(i->get<0>());
        }
    }
}

const ScaffoldGraph::Edges& ScaffoldGraph::getFroms(SuperPathId pTo) const
{
    if (!mNodes.count(pTo))
    {
        dumpNodes(cerr);
    }
    BOOST_ASSERT(mNodes.count(pTo));
    return mNodes.find(pTo)->second.mFrom;
}

const ScaffoldGraph::Edges& ScaffoldGraph::getTos(SuperPathId pFrom) const
{
    BOOST_ASSERT(mNodes.count(pFrom));
    return mNodes.find(pFrom)->second.mTo;
}

void ScaffoldGraph::add(SuperPathId pFrom, SuperPathId pTo, int64_t pGap, uint64_t pCount, int64_t pRange)
{
    mNodes[pFrom].mTo.push_back(Edge(pTo, pGap, pCount, pRange));
    mNodes[pTo].mFrom.push_back(Edge(pFrom, pGap, pCount, pRange));
}

void ScaffoldGraph::mergeEdge(SuperPathId pFrom, SuperPathId pTo, int64_t pGap, uint64_t pCount, int64_t pRange)
{
    // cerr << "m\t" << pFrom.value() << '\t' << pTo.value() << '\n';
    bool added = false; 
    Edges& tos(mNodes[pFrom].mTo);
    for (Edges::iterator i = tos.begin(); i != tos.end(); ++i)
    {
        if (i->get<0>() == pTo)
        {
            i->get<1>() = (i->get<1>() + pGap) / 2;
            i->get<2>() += pCount;
            i->get<3>() = (i->get<3>() + pRange) / 2;
            added = true;
            break;
        }
    }
    if (!added)
    {
        // If it wasn't there in one direction, it won't be there in the other.
        add(pFrom, pTo, pGap, pCount, pRange);
        return;
    }

    Edges& froms(mNodes[pTo].mFrom);
    for (Edges::iterator i = froms.begin(); i != froms.end(); ++i)
    {
        if (i->get<0>() == pFrom)
        {
            i->get<1>() = (i->get<1>() + pGap) / 2;
            i->get<2>() = pCount;
            i->get<3>() = (i->get<3>() + pRange) / 2;
            break;
        }
    }
}

// Removes an edge.
void ScaffoldGraph::remove(SuperPathId pFrom, SuperPathId pTo)
{
    removeTo(pFrom, pTo);
    removeFrom(pFrom, pTo);
}

// Removes a node.
void ScaffoldGraph::remove(SuperPathId pA)
{
    // cerr << "remove " << pA.value() << '\n';
    Node& n(mNodes[pA]);
    for (Edges::iterator i = n.mFrom.begin(); i != n.mFrom.end(); ++i)
    {
        removeTo(i->get<0>(), pA);
    }
    for (Edges::iterator i = n.mTo.begin(); i != n.mTo.end(); ++i)
    {
        removeFrom(pA, i->get<0>());
    }

    mNodes.erase(pA);
}

void ScaffoldGraph::removeTo(SuperPathId pFrom, SuperPathId pTo)
{
    BOOST_ASSERT(mNodes.find(pFrom) != mNodes.end());
    Node& n(mNodes[pFrom]);
    for (Edges::iterator i = n.mTo.begin(); i != n.mTo.end(); ++i)
    {
        if (i->get<0>() == pTo)
        {
            n.mTo.erase(i);
            return;
        }
    }
}

void ScaffoldGraph::removeFrom(SuperPathId pFrom, SuperPathId pTo)
{
    BOOST_ASSERT(mNodes.find(pTo) != mNodes.end());
    Node& n(mNodes[pTo]);
    for (Edges::iterator i = n.mFrom.begin(); i != n.mFrom.end(); ++i)
    {
        if (i->get<0>() == pFrom)
        {
            n.mFrom.erase(i);
            return;
        }
    }
}

// Returns the next scaffold file basename given a supergraph basename.
string 
ScaffoldGraph::nextScafBaseName(const GossCmdContext& pCxt, const std::string& pSgBaseName)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    for (uint64_t n = 0; ; ++n)
    {
        string basename = pSgBaseName + "-scaf." + lexical_cast<string>(n);
        if (!fac.exists(basename + ".header"))
        {
            return basename;
        }

        if (n == 1024)
        {
            log(warning, "suspiciously large number of scaffold files detected (at least 1024)");
        }
    }
}

// Deletes all of the scaffold files associated with the given supergraph basename.
void 
ScaffoldGraph::removeScafFiles(const GossCmdContext& pCxt, const std::string& pSgBaseName)
{
    FileFactory& fac(pCxt.fac);
    for (uint64_t n = 0; ; ++n)
    {
        string basename = pSgBaseName + "-scaf." + lexical_cast<string>(n);
        if (!fac.exists(basename + ".header"))
        {
            return;
        }
        fac.remove(basename + ".header");
        fac.remove(basename + ".links");
    }
}

// True if there are any scaffold files associated with the supergraph basename.
// N.B. really just checks for the first possibility.
bool 
ScaffoldGraph::existScafFiles(const GossCmdContext& pCxt, const std::string& pSgBaseName)
{
    string header0 = pSgBaseName + "-scaf.0.header";
    return pCxt.fac.exists(header0);
}

unique_ptr<ScaffoldGraph> ScaffoldGraph::read(const string& pName, FileFactory& pFactory,
                                            uint64_t pMinLinkCount)
{
    typedef vector<LinkReaderPtr> Readers;
    unique_ptr<ScaffoldGraph> scaf(new ScaffoldGraph);
    Readers ins;

    // Open all scaffold files.
    for (uint64_t n = 0; ;++n)
    {
        string basename = pName + "-scaf." + lexical_cast<string>(n);
        if (!pFactory.exists(basename + ".header"))
        {
            // Opened all files.
            break;
        }
        ins.push_back(LinkReaderPtr(new LinkReader(basename, pFactory)));
    }

    if (ins.size() == 0)
    {
        return scaf;
    }

    if (ins.size() == 1)
    {
        // No merging.
        for (LinkReader& r(*ins[0]); r.valid(); ++r)
        {
            LinkReader::Link l = *r;
            int64_t minGap = -l.mGapRange / 2;
            if (l.mCount >= pMinLinkCount && l.mGap >= minGap)
            {
                scaf->add(l.mLhs, l.mRhs, l.mGap, l.mCount, l.mGapRange);
            }
        }
        return scaf;
    }

    LinkReader::LinkLt lt;
    LinkReader::LinkEq eq;
    LinkReader::Link minL = **ins[0];
    for (uint64_t i = 1; i < ins.size(); ++i)
    {
        LinkReader& r(*ins[i]);
        if (r.valid())
        {
            const LinkReader::Link& l(*r);
            if (lt(l, minL))
            {
                minL = l;
            }
        }
    }

    while (!ins.empty())
    {
        int64_t gap = 0;
        int64_t gapRange = 0;
        uint64_t num = 0;

        for (Readers::iterator i = ins.begin(); i != ins.end(); )
        {
            LinkReader& r(**i);
            if (!r.valid())
            {
                i = ins.erase(i);
                continue;
            }

            const LinkReader::Link& l(*r);
            if (eq(l, minL))
            {
                gap += l.mCount * l.mGap;
                gapRange += l.mCount * l.mGapRange;
                num += l.mCount;
                ++r;
            }
            ++i;
        }

        if (num >= pMinLinkCount)
        {
            BOOST_ASSERT(num);
            gap = gap / int64_t(num);
            gapRange = gapRange / int64_t(num);
            int64_t minGap = -gapRange / 2;
            if (gap >= minGap)
            {
                scaf->add(minL.mLhs, minL.mRhs, gap, num, gapRange);
            }
        }

        // Find the next minL.
        bool set = false;
        for (Readers::iterator i = ins.begin(); i != ins.end(); )
        {
            LinkReader& r(**i);
            if (!r.valid())
            {
                i = ins.erase(i);
            }
            else
            {
                const LinkReader::Link& l(*r);
                if (!set || lt(l, minL))
                {
                    set = true;
                    minL = l;
                }
                ++i;
            }
        }
    }

    return scaf;
}

ScaffoldGraph::ScaffoldGraph()
    : mNodes()
{
}

void ScaffoldGraph::addDummyRcNodes(const SuperGraph& pSg)
{
    unordered_set<SuperPathId> add;
    for (ScaffoldGraph::NodeMap::const_iterator i = mNodes.begin(); i != mNodes.end(); ++i)
    {
        SuperPathId n = i->first;
        SuperPathId nRc = pSg.reverseComplement(n);

        if (!pSg.valid(n))
        {
            cerr << "invalid\t" << n.value() << '\n';
        }
        if (!pSg.valid(nRc))
        {
            cerr << "invalid rc\t" << nRc.value() << '\n';
        }

        if (!hasNode(nRc))
        {
            add.insert(nRc);
        }
    }

    for (unordered_set<SuperPathId>::const_iterator i = add.begin(); i != add.end(); ++i)
    {
        mNodes.insert(make_pair(*i, ScaffoldGraph::Node()));
    }
}

// Combine reverse complement nodes.
// NOTE: Assumes fwd and rc nodes are NOT in the same component!
void ScaffoldGraph::mergeRcs(const SuperGraph& pSg)
{
    addDummyRcNodes(pSg);
    unordered_set<SuperPathId> left;
    getNodes(left);

    while (!left.empty())
    {
        // Process an entire component at a time.
        SuperPathId seed(*left.begin());

        // Check if the component contains rcs.
        {
            unordered_set<SuperPathId> cmp;
            getConnectedNodes(seed, cmp);
            bool skip = false;
            for (unordered_set<SuperPathId>::const_iterator i = cmp.begin(); i != cmp.end(); ++i)
            {
                if (cmp.count(pSg.reverseComplement(*i)))
                {
                    skip = true;
                    for (i = cmp.begin(); i != cmp.end(); ++i)
                    {
                        left.erase(*i);
                    }
                    break;
                }
            }
            if (skip)
            {
                continue;
            }
        }

        vector<SuperPathId> stack(1, seed);
        while (!stack.empty())
        {
            SuperPathId n = stack.back();
            stack.pop_back();
            if (left.count(n) == 0)
            {
                continue;
            }

            // Add follow-up nodes.
            const ScaffoldGraph::Edges& froms(getFroms(n));
            const ScaffoldGraph::Edges& tos(getTos(n));
            for (ScaffoldGraph::Edges::const_iterator i = froms.begin(); i != froms.end(); ++i)
            {
                stack.push_back(i->get<0>());
            }
            for (ScaffoldGraph::Edges::const_iterator i = tos.begin(); i != tos.end(); ++i)
            {
                stack.push_back(i->get<0>());
            }

            SuperPathId nRc = pSg.reverseComplement(n);
            left.erase(n);
            left.erase(nRc);
         
            // cerr << "at\t" << n.value() << '\t' << nRc.value() << '\n';

            // Incorporate all rcFroms into tos.
            const ScaffoldGraph::Edges& rcFroms(getFroms(nRc));
            for (ScaffoldGraph::Edges::const_iterator i = rcFroms.begin(); i != rcFroms.end(); ++i)
            {
                SuperPathId from = n;
                SuperPathId to = pSg.reverseComplement(i->get<0>());
                int64_t gap = i->get<1>();
                uint64_t c = i->get<2>();
                int64_t gapRange = i->get<3>();
                mergeEdge(from, to, gap, c, gapRange);
                stack.push_back(to);
            }

            // Incorporate all rcTos into froms.
            const ScaffoldGraph::Edges& rcTos(getTos(nRc));
            for (ScaffoldGraph::Edges::const_iterator i = rcTos.begin(); i != rcTos.end(); ++i)
            {
                SuperPathId to = n;
                SuperPathId from = pSg.reverseComplement(i->get<0>());
                int64_t gap = i->get<1>();
                uint64_t c = i->get<2>();
                int64_t gapRange = i->get<3>();
                mergeEdge(from, to, gap, c, gapRange);
                stack.push_back(from);
            }

            // cerr << "rm\t" << nRc.value() << '\n';
            remove(nRc);
        }
    }
}


#if 0
    // Eliminate components that contain the same node in forward and rc form.
    void dropMirrored(const SuperGraph& pSg, ScaffoldGraph& pScaf, Logger& pLog)
    {
        uint64_t elimComps = 0;
        uint64_t elimNodes = 0;
        unordered_set<SuperPathId> left;
        pScaf.getNodes(left);
        while (!left.empty())
        {
            SuperPathId n = *left.begin();
            unordered_set<SuperPathId> cmp;
            pScaf.getConnectedNodes(n, cmp);
            bool bad = false;
            uint64_t numBad = 0;
            for (unordered_set<SuperPathId>::const_iterator i = cmp.begin(); i != cmp.end(); ++i)
            {
                if (cmp.count(pSg.reverseComplement(*i)))
                {
                    // Bad component.
                    cerr << "bad component containing " << (*i).value() << ", rc " 
                         << pSg.reverseComplement(*i).value()
                         << " (" << cmp.size() << " nodes)\n";
                    bad = true;
                    numBad += 1;
                    // break;
                }
            }
            if (bad)
            {
                elimComps += 1;
                elimNodes += cmp.size();
                cerr << "numBad: " << numBad << '\n';
            }
            for (unordered_set<SuperPathId>::const_iterator j = cmp.begin(); j != cmp.end(); ++j)
            {
                SuperPathId m(*j);
                left.erase(m);
                if (bad)
                {
                    pScaf.remove(m);
                }
            }
        }
        LOG(pLog, info) << "eliminated " << elimNodes << " nodes in "
                        << elimComps << " components";
    }

    class LenLt
    {
    public:

        bool operator()(const SuperPathId& pA, const SuperPathId& pB)
        {
            return mSg.baseSize(pA) < mSg.baseSize(pB);
        }
    
        LenLt(const SuperGraph& pSg)
            : mSg(pSg)
        {
        }

    private:
        const SuperGraph& mSg;

    };


    // Remove nodes from the scaffold graph which have contradictory predicted (A*)
    // and structural uniqueness.
    // TODO: Consider removing the shortest node in each structural motif.
    void elimNonUnique(const SuperGraph& pSg, ScaffoldGraph& pScaf, Logger& pLog)
    {
        LenLt lt(pSg);
        uint64_t numElim = 0;
        unordered_set<SuperPathId> nodes;
        pScaf.getNodes(nodes);
        for (unordered_set<SuperPathId>::iterator i = nodes.begin(); i != nodes.end(); )
        {
            SuperPathId n = *i;
            SuperGraph::Node e(pSg.end(n));
            SuperGraph::SuperPathIds ps;
            pSg.successors(e, ps);
            // n is unique, but has >1 successor.
            if (ps.size() > 1)
            {
                // cerr << "non-unique " << n.value() << "(" << ps.size() << ")\n";
/*
                ps.push_back(n);
                sort(ps.begin(), ps.end(), lt);
                for (uint64_t j = 0; j < ps.size(); ++j)
                {
                    cerr << ps[j].value() << ' ';
                }
                cerr << '\n';
                for (uint64_t j = 0; j < ps.size(); ++j)
                {
                    if (nodes.count(ps[j]))
                    {
                        cerr << "removing " << ps[j].value() << '\n';
                        pScaf.remove(ps[j]);
                        i = nodes.erase(i);
                        break;
                    }
                }
*/
                pScaf.remove(n);
                i = nodes.erase(i);
                numElim += 1;
                continue;
            }

            SuperPathId nRc = pSg.reverseComplement(n);
            e = pSg.end(nRc);
            pSg.successors(e, ps);
            // n is unique, but has >1 predecessor.
            if (ps.size() > 1)
            {
                // cerr << "non-unique " << n.value() << "(" << ps.size() << ")\n";

/*
                ps.push_back(n);
                sort(ps.begin(), ps.end(), lt);
                for (uint64_t j = 0; j < ps.size(); ++j)
                {
                    cerr << ps[j].value() << ' ';
                }
                cerr << '\n';
                for (uint64_t j = 0; j < ps.size(); ++j)
                {
                    if (nodes.count(ps[j]))
                    {
                        cerr << "removing " << ps[j].value() << '\n';
                        pScaf.remove(ps[j]);
                        i = nodes.erase(i);
                        break;
                    }
                }
*/

                pScaf.remove(n);
                i = nodes.erase(i);
                numElim += 1;
                continue;
            }

            ++i;
        }

        LOG(pLog, info) << "eliminated " << numElim << " nodes";
    }
#endif
