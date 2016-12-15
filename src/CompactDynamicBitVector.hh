// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef COMPACTDYNAMICBITVECTOR_HH
#define COMPACTDYNAMICBITVECTOR_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

class CompactDynamicBitVector
{
public:
    class Node;
    typedef std::shared_ptr<Node> NodePtr;

    class Visitor
    {
    public:
        virtual void enter() = 0;

        virtual void exit() = 0;

        virtual void leaf(uint64_t pWord) = 0;

        virtual ~Visitor() {}
    };

    class Node
    {
    public:
        virtual int64_t height() const = 0;

        virtual uint64_t size() const = 0;

        virtual uint64_t count() const = 0;

        virtual bool access(uint64_t pPos) const = 0;

        virtual uint64_t rank(uint64_t pPos) const = 0;

        virtual uint64_t select(uint64_t pRnk) const = 0;

        virtual NodePtr update(NodePtr& pSelf, uint64_t pPos, bool pBit) = 0;

        virtual NodePtr insert(NodePtr& pSelf, uint64_t pPos, bool pBit) = 0;

        virtual NodePtr erase(NodePtr& pSelf, uint64_t pPos) = 0;

        virtual void visit(Visitor& pVis) const = 0;

        virtual ~Node() {}
    };

    class Internal : public Node
    {
    public:
        int64_t height() const;

        uint64_t size() const;

        uint64_t count() const;

        bool access(uint64_t pPos) const;

        uint64_t rank(uint64_t pPos) const;

        uint64_t select(uint64_t pRnk) const;

        NodePtr update(NodePtr& pSelf, uint64_t pPos, bool pBit);

        NodePtr insert(NodePtr& pSelf, uint64_t pPos, bool pBit);

        NodePtr erase(NodePtr& pSelf, uint64_t pPos);

        void visit(Visitor& pVis) const
        {
            pVis.enter();
            mLhs->visit(pVis);
            mRhs->visit(pVis);
            pVis.exit();
        }

        Internal(NodePtr& pLhs, NodePtr& pRhs)
            : mLhs(pLhs), mRhs(pRhs)
        {
            recompute();
        }

    private:
        NodePtr rebalance(NodePtr& pSelf);

        void recompute()
        {
            mHeight = 1 + std::max(mLhs->height(), mRhs->height());
            mSize = mLhs->size() + mRhs->size();
            mCount = mLhs->count() + mRhs->count();
        }

        static NodePtr rotateLeft(NodePtr& pRoot)
        {
            NodePtr pPtr = pRoot;
            BOOST_ASSERT(dynamic_cast<const Internal*>(pPtr.get()));
            Internal& p(*static_cast<Internal*>(pPtr.get()));
            NodePtr a = p.mLhs;
            NodePtr qPtr = p.mRhs;
            BOOST_ASSERT(dynamic_cast<const Internal*>(qPtr.get()));
            Internal& q(*static_cast<Internal*>(qPtr.get()));
            NodePtr b = q.mLhs;
            NodePtr c = q.mRhs;
            p.mLhs = a;
            p.mRhs = b;
            p.recompute();
            q.mLhs = pRoot;
            q.mRhs = c;
            q.recompute();
            return qPtr;
        }

        static NodePtr rotateRight(NodePtr& pRoot)
        {
            NodePtr qPtr = pRoot;
            BOOST_ASSERT(dynamic_cast<const Internal*>(qPtr.get()));
            Internal& q(*static_cast<Internal*>(qPtr.get()));
            NodePtr pPtr = q.mLhs;
            BOOST_ASSERT(dynamic_cast<const Internal*>(pPtr.get()));
            Internal& p(*static_cast<Internal*>(pPtr.get()));
            NodePtr a = p.mLhs;
            NodePtr b = p.mRhs;
            NodePtr c = q.mRhs;
            q.mLhs = b;
            q.mRhs = c;
            q.recompute();
            p.mLhs = a;
            p.mRhs = qPtr;
            p.recompute();
            return pPtr;
        }

        uint64_t mHeight;
        uint64_t mSize;
        uint64_t mCount;
        NodePtr mLhs;
        NodePtr mRhs;
    };

    class External : public Node
    {
    public:
        int64_t height() const;

        uint64_t size() const;

        uint64_t count() const;

        bool access(uint64_t pPos) const;

        uint64_t rank(uint64_t pPos) const;

        uint64_t select(uint64_t pRnk) const;

        NodePtr update(NodePtr& pSelf, uint64_t pPos, bool pBit);

        NodePtr insert(NodePtr& pSelf, uint64_t pPos, bool pBit);

        NodePtr erase(NodePtr& pSelf, uint64_t pPos);

        void visit(Visitor& pVis) const
        {
            pVis.leaf(mWord);
        }

        External(uint64_t pWord)
            : mWord(pWord)
        {
        }

    private:
        uint64_t mWord;
    };

    uint64_t size() const
    {
        return mRoot->size();
    }

    uint64_t count() const
    {
        return mRoot->count();
    }

    bool access(uint64_t pPos) const
    {
        return mRoot->access(pPos);
    }

    uint64_t rank(uint64_t pPos) const
    {
        return mRoot->rank(pPos);
    }

    uint64_t select(uint64_t pRnk) const
    {
        return mRoot->select(pRnk);
    }

    void update(uint64_t pPos, bool pBit)
    {
        mRoot = mRoot->update(mRoot, pPos, pBit);
        //std::cerr << "height() = " << mRoot->height() << std::endl;
    }

    void insert(uint64_t pPos, bool pBit)
    {
        mRoot = mRoot->insert(mRoot, pPos, pBit);
        //std::cerr << "height() = " << mRoot->height() << std::endl;
    }

    void erase(uint64_t pPos)
    {
        mRoot = mRoot->erase(mRoot, pPos);
    }

    void save(const std::string& pBaseName, FileFactory& pFactory) const;

    CompactDynamicBitVector()
        : mRoot(new External(0))
    {
    }

    CompactDynamicBitVector(uint64_t mSize);

private:
    NodePtr mRoot;
};

#endif // COMPACTDYNAMICBITVECTOR_HH
