// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GRAPHESSENTIALS_HH
#define GRAPHESSENTIALS_HH

#ifndef TAGGEDNUM_HH
#include "TaggedNum.hh"
#endif

#ifndef SMALLBASEVECTOR_HH
#include "SmallBaseVector.hh"
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif

#ifndef RANKSELECT_HH
#include "RankSelect.hh"
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef BOOST_CAST_HPP_INCLUDED
#include <boost/cast.hpp>
#define BOOST_CAST_HPP_INCLUDED
#endif

template <typename Parent>
class GraphEssentials
{
public:

    // An edge is a k+1-mer present in the read set.
    // By definition it connects the nodes implied by
    // it's first k and last k bases.
    //
    struct EdgeTag {};
    typedef TaggedNum<EdgeTag,Gossamer::position_type> Edge;

    // A node is a k-mer present in the read set.
    // Nodes are not stored explicitly, but rather
    // are implied by the existence of edges that
    // have them as the first or last k bases.
    //
    struct NodeTag {};
    typedef TaggedNum<NodeTag,Gossamer::position_type> Node;

    // The node from which the given edge proceeds.
    //
    Node from(const Edge& pEdge) const
    {
        return Node(pEdge.value() >> 2);
    }

    // The node to which the given edge points.
    //
    Node to(const Edge& pEdge) const
    {
        return Node(pEdge.value() & mM);
    }

    // Return the number of edges that point to this node.
    //
    uint64_t inDegree(const Node& pNode) const
    {
        return outDegree(reverseComplement(pNode));
    }

    // Return the number of edges that originate at this node.
    //
    uint64_t outDegree(const Node& pNode) const
    {
        using Gossamer::rank_type;
        std::pair<rank_type,rank_type> r = beginEndRank(pNode);
        return r.second - r.first;
    }

    // Return the ranks of the first edge of the given node and the first
    // edge following the edges of the given node.
    //
    std::pair<Gossamer::rank_type,Gossamer::rank_type> beginEndRank(const Node& pNode) const
    {
        Gossamer::position_type v = pNode.value() << 2;
        std::pair<Gossamer::rank_type,Gossamer::rank_type> r
            = parent().rank(Edge(v), Edge(v + 4));
        return r;
    }

    // Return the rank of the first edge of the given node.
    //
    Gossamer::rank_type beginRank(const Node& pNode) const
    {
        Gossamer::position_type v = pNode.value() << 2;
        return parent().rank(Edge(v));
    }

    Node reverseComplement(const Node& pNode) const
    {
        Gossamer::position_type val = pNode.value();
        val.reverseComplement(parent().K());
        return Node(val);
    }

    Edge reverseComplement(const Edge& pEdge) const
    {
        Gossamer::position_type val = pEdge.value();
        val.reverseComplement(parent().K() + 1);
        return Edge(val);
    }

    // Append the sequence for this node to the
    // sequence object.
    //
    void seq(const Node& pNode, SmallBaseVector& pVec) const
    {
        Gossamer::position_type s = pNode.value();
        s.reverse();
        s >>= std::numeric_limits<Gossamer::position_type>::digits - 2 * parent().K();
        for (uint64_t i = 0; i < parent().K(); ++i)
        {
            pVec.push_back(s & 3);
            s >>= 2;
        }
    }

    // Append the sequence for this edge to the
    // sequence object.
    //
    void seq(const Edge& pEdge, SmallBaseVector& pVec) const
    {
        seq(from(pEdge), pVec);
        pVec.push_back(pEdge.value() & 3);
    }

    Node normalize(const Node& pNode) const
    {
        Node n(pNode);
        n.value().normalize(parent().K());
        return n;
    }

    Edge normalize(const Edge& pEdge) const
    {
        Edge e(pEdge);
        e.value().normalize(parent().K() + 1);
        return e;
    }

    /**
     * Returns true iff the given node is canonical.
     */
    bool canonical(const Node& pNode) const
    {
        return pNode == normalize(pNode);
    }

    /**
     * Returns true iff the given node is anti-canonical.
     */
    bool antiCanonical(const Node& pNode) const
    {
        return !canonical(pNode);
    }

    /**
     * Returns true iff the given edge is canonical.
     */
    bool canonical(const Edge& pEdge) const
    {
        return pEdge == normalize(pEdge);
    }

    /**
     * Returns true iff the given edge is anti-canonical.
     */
    bool antiCanonical(const Edge& pEdge) const
    {
        return !canonical(pEdge);
    }

protected:
    Gossamer::position_type mM;

private:
    const Parent& parent() const
    {
        return static_cast<const Parent&>(*this);
    }
};

#endif // GRAPHESSENTIALS_HH
