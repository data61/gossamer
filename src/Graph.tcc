// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef STD_QUEUE
#include <queue>
#define STD_QUEUE
#endif

// For the given edge, find the terminating edge
// in the sequence of edges starting with the given
// edge such that neither the forwards nor reverse
// complement paths contain any branches.
//
template <typename Visitor>
Graph::Edge
Graph::linearPath(const Edge& pBegin, Visitor& pVis) const
{
    Edge e = pBegin;
    uint64_t eRank = rank(e);
    Node n = to(e);
    std::pair<uint64_t,uint64_t> r = beginEndRank(n);
    while (r.second - r.first == 1 && inDegree(n) == 1)
    {
        Edge ee = select(r.first);
        if (ee == pBegin)
        {
            break;
        }
        if (!pVis(e, eRank))
        {
            return e;
        }
        e = ee;
        eRank = r.first;
        n = to(e);
        r = beginEndRank(n);
    }

    pVis(e, eRank);
    return e;
}


template <typename Visitor>
void
Graph::visitPath(const Edge& pBegin, const Edge& pEnd, Visitor& pVis) const
{
    Edge e = pBegin;
    //Node n = to(e);
    Gossamer::rank_type r = rank(e);
    while (e != pEnd)
    {
        pVis(e, r);
        Node n = to(e);
        e = onlyOutEdge(n, r);
    }
    pVis(e, r);
}


template <typename Visitor>
void
Graph::visitMajorityPath(const Node& pBegin, uint64_t pLength, Visitor& pVis) const
{
    Node n = pBegin;
    uint64_t i = K();
    while (outDegree(n) > 0 && i < pLength)
    {
        Edge e = majorityEdge(n);
        pVis(e);
        ++i;
        n = to(e);
    }
}

template <typename Visitor>
void
Graph::inTerminals(Visitor& pVisitor) const
{
    for (Gossamer::rank_type i = 0; i < mEdgesView.count(); ++i)
    {
        Node n = from(select(i));
        if (inDegree(n) == 0)
        {
            pVisitor(n);
        }
    }
}

template <typename Visitor>
void
Graph::linearSegments(Visitor& pVisitor) const
{
    using Gossamer::rank_type;
    using boost::numeric_cast;

    boost::dynamic_bitset<> seen(mEdgesView.count());
    for (rank_type i = 0; i < mEdgesView.count(); ++i)
    {
        if (seen[numeric_cast<uint64_t>(i)])
        {
            continue;
        }
        Edge b = select(i);
        if (inDegree(from(b)) == 1 && outDegree(to(b)) == 1)
        {
            continue;
        }
        Edge e = linearPath(b);
        MarkSeen ms(*this, seen);
        visitPath(b, e, ms);
        visitPath(reverseComplement(e), reverseComplement(b), ms);
        pVisitor(b, e);
    }
    std::cerr << "after main traversal:\n";
    std::cerr << "mEdgesView.count() = " << mEdgesView.count() << std::endl;
    std::cerr << "seen.count() = " << seen.count() << std::endl;
    for (rank_type i = 0; i < mEdgesView.count(); ++i)
    {
        if (seen[numeric_cast<uint64_t>(i)])
        {
            continue;
        }
        Edge b = select(i);
        Edge e = linearPath(b);
        MarkSeen ms(*this, seen);
        visitPath(b, e, ms);
        visitPath(reverseComplement(e), reverseComplement(b), ms);
        pVisitor(b, e);
    }
    std::cerr << "after second traversal:\n";
    std::cerr << "mEdgesView.count() = " << mEdgesView.count() << std::endl;
    std::cerr << "seen.count() = " << seen.count() << std::endl;
}

template <typename Visitor>
void
Graph::dfs(Visitor& pVis) const
{
    using Gossamer::rank_type;

    boost::dynamic_bitset<> seen(mEdgesView.count());

    if (mEdgesView.count() == 0)
    {
        return;
    }

    rank_type i = 0;
    Node n(from(select(i)));
    dfs(n, seen, pVis);

    Node p = n;
    for (++i; i < mEdgesView.count(); ++i)
    {
        n = from(select(i));
        if (n == p)
        {
            continue;
        }
        dfs(n, seen, pVis);
        p = n;
    }
}

template <typename Visitor>
void
Graph::dfs(const Node& pBegin, boost::dynamic_bitset<>& pSeen, Visitor& pVis) const
{
    using Gossamer::rank_type;
    using boost::numeric_cast;

    std::vector<Node> stk;
    stk.push_back(pBegin);

    while (!stk.empty())
    {
        Node n = stk.back();
        stk.pop_back();

        std::pair<rank_type,rank_type> r = beginEndRank(n);
        rank_type r0 = r.first;
        rank_type r1 = r.second;
        for (rank_type i = r0; i < r1; ++i)
        {
            if (pSeen[numeric_cast<uint64_t>(i)])
            {
                continue;
            }
            pSeen[numeric_cast<uint64_t>(i)] = 1;
            Edge e = select(i);
            bool cont = pVis(e);
            if (cont)
            {
                stk.push_back(to(e));
            }
        }
    }
}

template <typename Visitor>
void
Graph::bfs(const Node& pBegin, boost::dynamic_bitset<>& pSeen, Visitor& pVis) const
{
    using Gossamer::rank_type;
    using boost::numeric_cast;

    std::queue<Node> que;
    que.push(pBegin);

    while (!que.empty())
    {
        Node n = que.front();
        que.pop();

        std::pair<rank_type,rank_type> r = beginEndRank(n);
        rank_type r0 = r.first;
        rank_type r1 = r.second;
        for (rank_type i = r0; i < r1; ++i)
        {
            if (pSeen[numeric_cast<uint64_t>(i)])
            {
                continue;
            }
            pSeen[numeric_cast<uint64_t>(i)] = 1;
            Edge e = select(i);
            bool cont = pVis(e);
            if (cont)
            {
                que.push(to(e));
            }
        }
    }
}

