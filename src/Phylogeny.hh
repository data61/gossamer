// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PHYLOGENY_HH
#define PHYLOGENY_HH

#ifndef ANNOTTREE_HH
#include "AnnotTree.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#include <set>
#include <iostream>
#include <vector>
#include <unordered_map>

class Phylogeny
{
public:
    uint32_t root() const
    {
        return mRootNode;
    }

    const AnnotTree::NodePtr& getNode(uint32_t pNode) const
    {
        BOOST_ASSERT(mNodeIndex.find(pNode) != mNodeIndex.end());
        return mNodeIndex.find(pNode)->second;
    }

    AnnotTree::NodePtr& getNode(uint32_t pNode)
    {
        BOOST_ASSERT(mNodeIndex.find(pNode) != mNodeIndex.end());
        return mNodeIndex.find(pNode)->second;
    }

    const std::vector<uint32_t>& kids(uint32_t pNode) const
    {
        std::unordered_map<uint32_t,std::vector<uint32_t> >::const_iterator i = mChildIndex.find(pNode);
        if (i != mChildIndex.end())
        {
            return i->second;
        }
        return mEmptyKids;
    }

    /**
     * Find the lowest common ancestor of a set of nodes in the tree.
     * Returns 0 is there is no common ancestor.
     */
    uint32_t lca(const std::set<uint32_t>& pNodes) const
    {
        BOOST_ASSERT(pNodes.size() > 0);
        std::vector<uint32_t> nodes(pNodes.begin(), pNodes.end());
        uint32_t n = nodes[0];
        for (uint32_t i = 1; i < nodes.size(); ++i)
        {
            n = lca(n, nodes[i]);
        }
        return n;
    }

    uint32_t lca(uint32_t pLhs, uint32_t pRhs) const
    {
        // Create the acestor vector for pLhs
        std::vector<uint32_t> l;
        ancestors(pLhs, l);

        // Create the acestor vector for pRhs
        std::vector<uint32_t> r;
        ancestors(pRhs, r);

        std::vector<uint32_t>::const_reverse_iterator lItr = l.rbegin();
        std::vector<uint32_t>::const_reverse_iterator rItr = r.rbegin();
        uint32_t n = 0;
        while (lItr != l.rend() && rItr != r.rend())
        {
            if (*lItr != *rItr)
            {
                break;
            }
            n = *lItr;
            ++lItr;
            ++rItr;
        }
        return n;
    }

    void ancestors(const uint32_t& pNode, std::vector<uint32_t>& pAncestorSet) const
    {
        uint32_t prevNode = pNode;
        uint32_t n = pNode;
        do
        {
            pAncestorSet.push_back(n);
            prevNode = n;
            BOOST_ASSERT(mParentIndex.find(n) != mParentIndex.end());
            n = mParentIndex.find(n)->second;
        }
        while (prevNode != n);
    }

    void read(const std::string& pFileName, FileFactory& pFactory);

    void write(std::ostream& pOut) const;

    Phylogeny()
    {
    }

private:
    uint32_t index(const AnnotTree::NodePtr& pNode);

    AnnotTree::NodePtr mRoot;
    uint32_t mRootNode;
    std::vector<uint32_t> mEmptyKids;
    std::unordered_map<uint32_t,uint32_t> mParentIndex;
    std::unordered_map<uint32_t,std::vector<uint32_t> > mChildIndex;
    std::unordered_map<uint32_t,std::string> mNameIndex;
    std::unordered_map<uint32_t,AnnotTree::NodePtr> mNodeIndex;
};

#endif // PHYLOGENY_HH
