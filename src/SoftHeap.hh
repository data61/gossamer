// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SOFTHEAP_HH
#define SOFTHEAP_HH

class SoftHeap
{
public:
    typedef uint64_t value_type;

    static const uint64_t E_r = 10;
    static const uint64_t Log2E_r = 4;
    static const uint64_t R = Log2E_r + 5;

    struct Node;
    typedef std::shared_ptr<Node> NodePtr;

    class Node
    {
    public:
        uint64_t rank() const
        {
            return mRank;
        }

        void sift()
        {
            while (mList.size() < mSize && !leaf())
            {
                if (!mLeft)
                {
                    std::swap(mLeft, mRight);
                }
                else if (mLeft->mCKey > mRight->mCkey)
                {
                    std::swap(mLeft, mRight);
                }
                mList.splice(mList.end(), mLeft->mList);
                mCKey = mLeft->mCKey;
                if (mLeft->leaf())
                {
                    mLeft = NodePtr();
                }
                else
                {
                    mLeft->sift();
                }
            }
        }

        static NodePtr combine(const NodePtr& pLeft, const NodePtr& pRight)
        {
            return NodePtr(new Node(pLeft, pRight));
        }

        Node(const value_type& pItem)
            : mRank(0), mSize(size(mRank, 0))
        {
            mList.push_back(pItem);
        }

        Node(const NodePtr& pLeft, const NodePtr& pRight)
            : mRank(pLeft->mRank + 1), mSize(size(mRank, mLeft->mSize)),
              mLeft(pLeft), mRight(pRight),
        {
            BOOST_ASSERT(pLeft);
            BOOST_ASSERT(!pRight || pLeft->mRank == pRight->mRank);
            sift();
        }

    private:

        bool leaf() const
        {
            return !mLeft && !mRight;
        }

        static uint64_t size(uint64_t k, uint64_t z)
        {
            if (k <= R)
            {
                return 1;
            }
            return (3 * z + 1) / 2;
        }

        const uint64_t mRank;
        const uint64_t mSize;
        NodePtr mLeft;
        NodePtr mRight;
        value_type mCKey;
        std::list<value_type> mList;
    };

    struct Tree;
    typedef std::shared_ptr<Tree> TreePtr;

    struct Tree
    {
        NodePtr root;
        TreePtr sufMin;

        uint64_t rank() const
        {
            BOOST_ASSERT(root);
            return root->rank();
        }

        static TreePtr combine(const TreePtr& pLhs, const TreePtr& pRhs)
        {
            return TreePtr(new Tree(Node::combine(pLhs->root, pRhs->root)));
        }

        Tree(const value_type& pItem)
        {
            root = NodePtr(new Node(pItem));
        }

        Tree(const NodePtr& pRoot)
            : root(pRoot)
        {
        }
    };

    void insert(const uint64_t& pKey);

    void remove(const uint64_t& pKey);

    void meld(SoftHeap& pRhs)
    {
        std::vector<TreePtr> ts;
        std::vector<TreePtr>::iterator l = mTrees.begin();
        std::vector<TreePtr>::iterator r = pRhs.mTrees.begin();
        while (l != mTrees.end() && r != pRhs.mTrees.end())
        {
            if ((*l)->rank() < (*r)->rank())
            {
                if (ts.size() && ts.back()->rank() == (*l)->rank())
                {
                    ts.back() = Tree::combine(ts.back(), *l);
                }
                else
                {
                    ts.push_back(*l);
                }
                ++l;
                continue;
            }
            if ((*r)->rank() < (*l)->rank())
            {
                if (ts.size() && ts.back()->rank() == (*r)->rank())
                {
                    ts.back() = Tree::combine(ts.back(), *r);
                }
                else
                {
                    ts.push_back(*r);
                }
                ++r;
                continue;
            }
            BOOST_ASSERT((*l)->rank() == (*r)->rank());
            if (ts.size() && ts.back()->rank() == (*l)->rank())
            {
                ts.push_back(Tree::combine(*l, *r));
                ++l;
                ++r;
                continue;
            }
        }
        uint64_t p = ts.size();
        while (l != mTrees.end())
        {
            ts.push_back(*l);
            ++l;
        }
        while (r != pRhs.mTrees.end())
        {
            ts.push_back(*r);
            ++r;
        }
        mTrees.swap(ts);
        updateSuffMin(p);
    }

    uint64_t min() const;

    SoftHeap(const value_type& pItem)
    {
        mTrees.push_back(TreePtr(new Tree(pItem)));
    }

private:
    std::vector<TreePtr> mTrees;
};

#endif // SOFTHEAP_HH
