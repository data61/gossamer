// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef FIBHEAP_HH
#define FIBHEAP_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef BOOST_CALL_TRAITS_HPP
#include <boost/call_traits.hpp>
#define BOOST_CALL_TRAITS_HPP
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef BOOST_UTILITY_HPP
#include <boost/utility.hpp>
#define BOOST_UTILITY_HPP
#endif

#ifndef BOOST_NONCOPYABLE_HPP
#include <boost/noncopyable.hpp>
#define BOOST_NONCOPYABLE_HPP
#endif

#ifndef STD_IOSTREAM
#include <iostream>
#define STD_IOSTREAM
#endif

#ifndef STD_DEQUE
#include <deque>
#define STD_DEQUE
#endif

// Enable TEST_FIB_HEAP to include code which should only be used
// while testing or debugging.

namespace detail {
    template<typename K, typename V>
    struct FibHeapNode : private boost::noncopyable
    {
        typedef K key_type; 
        typedef V value_type; 
        typedef FibHeapNode<K,V>* node_ptr; 
        typedef const FibHeapNode<K,V>* const_node_ptr; 

        key_type mKey;
        value_type mValue;
        uint64_t mDegree; // Number of children.
        bool mMark;

        node_ptr mPrev;
        node_ptr mNext;
        node_ptr mChild;
        node_ptr mParent;

        FibHeapNode(typename boost::call_traits<key_type>::param_type pKey,
                    typename boost::call_traits<value_type>::param_type pValue)
            : mKey(pKey), mValue(pValue),
              mDegree(0), mMark(false),
              mChild(0), mParent(0)
        {
            mPrev = mNext = this;
        }

        bool isSingleton() const
        {
            return (this == mNext);
        }

        void insertAfter(node_ptr pOther)
        {
            if (!pOther)
            {
                return;
            }
            mNext->mPrev = pOther->mPrev;
            pOther->mPrev->mNext = mNext;
            mNext = pOther;
            pOther->mPrev = this;
        }

        void remove()
        {
            mPrev->mNext = mNext;
            mNext->mPrev = mPrev;
            mNext = mPrev = this;
        }

        void addChild(node_ptr pOther)
        {
            if (!mChild)
            {
                mChild = pOther;
            }
            else
            {
                mChild->insertAfter(pOther);
            }
            pOther->mParent = this;
            pOther->mMark = false;
            ++mDegree;
        }

        void removeChild(node_ptr pOther)
        {
            if (pOther->mParent != this)
            {
                throw "Trying to remove a child from a non-parent.";
            }
            else if (pOther->isSingleton())
            {
                if (mChild != pOther)
                {
                    throw "Trying to remove a non-child.";
                }
                mChild = 0;
            }
            else
            {
                if (mChild == pOther)
                {
                    mChild = pOther->mNext;
                }
                pOther->remove();
            }
            pOther->mParent = 0;
            pOther->mMark = false;
            --mDegree;
        }

        uint64_t forceCount() const
        {
            if (!mChild)
            {
                return 1;
            }
            uint64_t count = mChild->forceCount() + 1;
            for (const_node_ptr c = mChild->mNext; c != mChild; c = c->mNext)
            {
                count += c->forceCount();
            }
            return count;
        }

#ifdef TEST_FIB_HEAP
        bool checkHeapInvariant() const
        {
            if (!mChild)
            {
                bool ok = mDegree == 0;
                if (!ok)
                {
                    std::cerr << "FibNode " << mValue << ": case 1 failed\n";
                }
                return ok;
            }
            uint64_t checkDegree = 1;

            if (!mChild->checkHeapInvariant())
            {
                return false;
            }

            if (mChild->mKey < mKey)
            {
                std::cerr << "FibNode " << mValue << ": case 2 failed\n";
                std::cerr << "    " << mChild->mKey << " < " << mKey << "\n";
                return false;
            }

            for (const_node_ptr c = mChild->mNext; c != mChild; c = c->mNext)
            {
                if (!c->checkHeapInvariant())
                {
                    return false;
                }
                if (c->mKey < mKey)
                {
                    std::cerr << "FibNode " << mValue << ": case 3 failed\n";
                    return false;
                }
                ++checkDegree;
            }
            bool ok = checkDegree == mDegree;
            if (!ok)
            {
                std::cerr << "FibNode " << mValue << ": case 4 failed\n";
                std::cerr << "checkDegree = " << checkDegree << "\n";
                std::cerr << "mDegree = " << mDegree << "\n";
            }
            return ok;
        }

        void dump(std::ostream& pOut) const
        {
            pOut << mKey << ':' << mValue << ':' << mDegree << ':' << mMark;
            if (mChild)
            {
                pOut << '(';
                const_node_ptr n = mChild;
                do
                {
                    n->dump(pOut);
                    pOut << ' ';
                    n = n->mNext;
                } while (n != mChild);
                pOut << ')';
            }
        }
#endif
    };
}

template<typename K, typename V>
class FibHeap : private boost::noncopyable
{
    typedef K key_type; 
    typedef V value_type; 
    typedef typename ::detail::FibHeapNode<K,V>::node_ptr node_ptr; 
    typedef typename ::detail::FibHeapNode<K,V>::const_node_ptr const_node_ptr; 

    node_ptr mRoot;
    uint64_t mCount;
    uint64_t mMaxDegree;

    node_ptr insertNode(node_ptr pNewNode)
    {
        if (!mRoot)
        {
            mRoot = pNewNode;
        }
        else
        {
            mRoot->insertAfter(pNewNode);
            if (pNewNode->mKey < mRoot->mKey)
            {
                mRoot = pNewNode;
            }
        }
        return pNewNode;
    }

    void promoteChildrenOfRoot()
    {
        if (mRoot->mChild)
        {
            node_ptr child = mRoot->mChild;
            do {
                child->mParent = 0;
                if (child->mDegree > mMaxDegree)
                {
                    mMaxDegree = child->mDegree;
                }
                child = child->mNext;
            } while (child != mRoot->mChild);
            mRoot->mChild = 0;
            mRoot->insertAfter(child);
        }
    }


    void cascadingCut(node_ptr pIt)
    {
        node_ptr parent = pIt->mParent;
        for (;;)
        {
            parent->removeChild(pIt);
            insertNode(pIt);
            if (!parent->mParent)
            {
                return;
            }
            else if (!parent->mMark)
            {
                parent->mMark = true;
                return;
            }
            else
            {
                pIt = parent;
                parent = parent->mParent;
            }
        }
    }

 
    void removeCurrentRoot()
    {
        if (!mRoot)
        {
            throw "No element to delete";
        }
        --mCount;
        promoteChildrenOfRoot();

        if (mRoot->mNext == mRoot)
        {
            BOOST_ASSERT(mCount == 0);
            node_ptr root = mRoot;
            mRoot = 0;
            delete root;
            return;
        }

        std::vector<node_ptr> newRoots(mMaxDegree + 1);
        node_ptr currentPtr = mRoot->mNext;
        mMaxDegree = 0; 
        do
        {
            uint64_t currentDegree = currentPtr->mDegree;
            node_ptr current = currentPtr;
            currentPtr = currentPtr->mNext;
            while (newRoots[currentDegree])
            {
                node_ptr other = newRoots[currentDegree];
                if (current->mKey > other->mKey)
                {
                    std::swap(other, current);
                }
                other->remove();
                current->addChild(other);
                newRoots[currentDegree] = 0;
                ++currentDegree;
                if (currentDegree + 1 >= newRoots.size())
                {
                    newRoots.push_back(0);
                }
            }
            newRoots[currentDegree] = current;
        } while (currentPtr != mRoot);

        {
            node_ptr root = mRoot;
            mRoot = 0;
            delete root;
        }

        uint64_t newMaxDegree = 0;

        for (uint64_t i = 0; i < newRoots.size(); ++i)
        {
            node_ptr newRoot = newRoots[i];
            if (newRoot)
            {
                newRoot->mNext = newRoot->mPrev = newRoot;
                insertNode(newRoot);
                if (i > newMaxDegree)
                {
                    newMaxDegree = i;
                }
            }
        }

        mMaxDegree = newMaxDegree;
    }

public:
    typedef node_ptr iterator;
    typedef const_node_ptr const_iterator;

    FibHeap()
        : mRoot(0), mCount(0), mMaxDegree(0)
    {    
    }    

    void clear()
    {
        if (mRoot)
        {
            while (mRoot->mNext != mRoot && !mRoot->isSingleton())
            {
                promoteChildrenOfRoot();
                node_ptr root = mRoot;
                mRoot = root->mNext;
                root->remove();
                delete root;
            }
           node_ptr root = mRoot;
           mRoot = 0;
           delete root;
        }
        mCount = 0;
        mMaxDegree = 0;
    }

    ~FibHeap()
    {
        clear();
    }

    uint64_t size() const
    {
        return mCount;
    }

    bool empty() const
    {
        return mCount == 0;
    }

    iterator insert(typename boost::call_traits<key_type>::param_type pKey,
                    typename boost::call_traits<value_type>::param_type pValue)
    {
        ++mCount;
        return insertNode(new ::detail::FibHeapNode<K,V>(pKey, pValue));
    }

    iterator minimum()
    {
        if (!mRoot)
        {
            throw "No minimum element";
        }
        return mRoot;
    }

    const_iterator minimum() const
    {
        if (!mRoot)
        {
            throw "No minimum element";
        }
        return mRoot;
    }

 
    void removeMinimum()
    {
        removeCurrentRoot();
    }

    void decreaseKey(iterator pIt,
                     typename boost::call_traits<key_type>::param_type pNewKey)
    {
        if (pNewKey > pIt->mKey)
        {
            throw "Trying to decrease key to a greater key.";
        }
        pIt->mKey = pNewKey;
        node_ptr parent = pIt->mParent;
        if (!parent)
        {
            // This is a root node. Just check to see if this is
            // a new minimum.
            if (pIt->mKey < mRoot->mKey)
            {
                mRoot = pIt;
            }
            return;
        }
        else if (parent->mKey <= pNewKey)
        {
            // Child still has a key greater than parent.
            return;
        }

        // Otherwise do a cascading cut.
        cascadingCut(pIt);
    }

    void remove(iterator pIt)
    {
        if (pIt->mParent)
        {
            // If the node has a parent, perform a cascading cut to
            // move it to the set of roots.
            cascadingCut(pIt);
        }
        mRoot = pIt;
        removeCurrentRoot();
    }

#ifdef TEST_FIB_HEAP
    bool checkHeapInvariant() const
    {
        if (!mRoot)
        {
            bool ok = !mMaxDegree && !mCount;
            if (!ok)
            {
                std::cerr << "FibHeap: case 1 failed\n";
            }
            return ok;
        }
        else if (!mRoot->checkHeapInvariant())
        {
            return false;
        }
        uint64_t checkMaxDegree = mRoot->mDegree;
        uint64_t checkCount = mRoot->forceCount();
        for (const_node_ptr c = mRoot->mNext; c != mRoot; c = c->mNext)
        {
            if (!c->checkHeapInvariant())
            {
                return false;
            }
            checkCount += c->forceCount();
            uint64_t degree = c->mDegree;
            if (degree > checkMaxDegree) checkMaxDegree = degree;
        }
        bool ok = mCount == checkCount;
        if (!ok)
        {
            std::cerr << "FibHeap: case 2 failed\n";
            std::cerr << "  mCount = " << mCount << "\n";
            std::cerr << "  checkCount = " << checkCount << "\n";
        }
        return ok;
    }
#endif

    struct Iterator
    {
        std::deque<const_iterator> mDeque;

        bool empty() const
        {
            return mDeque.empty();
        }

        const const_iterator operator*() const
        {
            return mDeque.front();
        }

        Iterator& operator++()
        {
            const_iterator children = mDeque.front()->mChild;
            mDeque.pop_front();
            if (children)
            {
                mDeque.push_front(children);
                for (const_iterator i = children->mNext; i != children; i = i->mNext)
                {
                    mDeque.push_front(i);
                }
            }
            return *this;
        }

        Iterator(const_iterator pRoot)
        {
            if (!pRoot)
            {
                return;
            }
            mDeque.push_back(pRoot);
            for (const_iterator i = pRoot->mNext; i != pRoot; i = i->mNext)
            {
                mDeque.push_back(i);
            }
        }
    };

    Iterator iterate() const
    {
        return Iterator(mRoot);
    }

#ifdef TEST_FIB_HEAP
    void dump(std::ostream& pOut) const
    {
        pOut << mMaxDegree << ' ' << mCount << "\n";
        if (mRoot)
        {
            const_node_ptr n = mRoot;
            do
            {
                n->dump(pOut);
                pOut << '\n';
                n = n->mNext;
            } while (n != mRoot);
        }
    }
#endif
};


#endif
