// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SPARSEARRAYVIEW_HH
#define SPARSEARRAYVIEW_HH

#ifndef SPARSEARRAY_HH
#include "SparseArray.hh"
#endif

#ifndef STRINGFILEFACTORY_HH
#include "StringFileFactory.hh"
#endif

class SparseArrayView
{
public:
    typedef SparseArray::rank_type rank_type;
    typedef SparseArray::position_type position_type;

private:
    class Mask : public WordyBitVector, public DenseRank, public DenseSelect
    {
    public:
        class Builder
        {
        public:
            void push_back(const rank_type& pPos)
            {
                mVec.push(pPos);
                mRnk.push_back(pPos);
                while (mCurrPos < pPos)
                {
                    mSel.push_back(mCurrPos++);
                }
                ++mCurrPos;
            }

            void end(const rank_type& pPos)
            {
                while (mCurrPos < pPos)
                {
                    mVec.push_backX(0);
                    mSel.push_back(mCurrPos);
                    ++mCurrPos;
                }
                mVec.end();
                mRnk.end(pPos);
                mSel.end();
            }

            Builder(const std::string& pName, FileFactory& pFactory)
                : mVec(pName + ".bits", pFactory),
                  mRnk(pName + ".rank-index", pFactory),
                  mSel(pName + ".select0-index", pFactory, true),
                  mCurrPos(0)
            {
            }

        private:
            WordyBitVector::Builder mVec;
            DenseRank::Builder mRnk;
            DenseSelect::Builder mSel;
            uint64_t mCurrPos;
        };

        uint64_t count() const
        {
            return mCount;
        }

        bool access(const rank_type& pPos) const
        {
            return rank(pPos + 1) - rank(pPos);
        }

        rank_type select0(const rank_type& pRnk) const
        {
            return DenseSelect::select(pRnk);
        }

        Mask(const std::string& pName, FileFactory& pFactory)
            : WordyBitVector(pName + ".bits", pFactory),
              DenseRank(*this, pName + ".rank-index", pFactory),
              DenseSelect(*this, pName + ".select0-index", pFactory, true),
              mCount(DenseRank::count())
        {
        }
    private:
        uint64_t mCount;
    };

public:

    class Iterator
    {
    public:
        bool valid() const
        {
            return mCurr < mView->count();
        }

        position_type operator*() const
        {
            return mView->select(mCurr);
        }

        void operator++()
        {
            ++mCurr;
        }

        Iterator(const SparseArrayView& pView)
            : mView(&pView), mCurr(0)
        {
        }

    private:
        const SparseArrayView* mView;
        uint64_t mCurr;
    };

    position_type size() const
    {
        return mArray.size();
    }
    
    rank_type count() const
    {
        if (!mMask.get())
        {
            return mArray.count();
        }
        return mArray.count() - mMask->count();
    }

    bool access(const position_type& pPos) const
    {
        if (!mMask.get())
        {
            return mArray.access(pPos);
        }
        rank_type r;
        return mArray.accessAndRank(pPos, r)
                && !mMask->access(r);
    }

    bool accessAndRank(const position_type& pPos, rank_type& pRank) const
    {
        if (!mMask.get())
        {
            return mArray.accessAndRank(pPos, pRank);
        }
        rank_type r;
        bool a = mArray.accessAndRank(pPos, r);
        rank_type s;
        bool m = mMask->accessAndRank(r, s);
        pRank = r - s;
        return a && !m;
    }

    std::pair<rank_type,rank_type> rank(const position_type& pLhs, const position_type& pRhs) const
    {
        if (!mMask.get())
        {
            return mArray.rank(pLhs, pRhs);
        }
        std::pair<rank_type,rank_type> a = mArray.rank(pLhs,pRhs);
        std::pair<rank_type,rank_type> m = mMask->rank(a.first,a.second);
        return std::pair<rank_type,rank_type>(a.first - m.first, a.second - m.second);
    }

    rank_type rank(const position_type& pPos) const
    {
        if (!mMask.get())
        {
            return mArray.rank(pPos);
        }
        rank_type a = mArray.rank(pPos);
        rank_type m = mMask->rank(a);
        return a - m;
    }

    rank_type originalRank(const rank_type& pRank) const
    {
        if (!mMask.get())
        {
            return pRank;
        }
        return mMask->select0(pRank);
    }

    position_type select(rank_type pRnk) const
    {
        if (!mMask.get())
        {
            return mArray.select(pRnk);
        }
        rank_type r = mMask->select0(pRnk);
        return mArray.select(r);
    }

    template <typename Itr>
    void remove(Itr& pRemovedItr)
    {
        if (!mMask.get())
        {
            {
                Mask::Builder b("mask", mFac);
                while (pRemovedItr.valid())
                {
                    b.push_back(*pRemovedItr);
                    ++pRemovedItr;
                }
                b.end(mArray.count());
            }
            mMask = std::unique_ptr<Mask>(new Mask("mask", mFac));
            return;
        }
        {
            Mask::Builder b("mask", mFac);
            WordyBitVector::Iterator1 itr(mMask->iterator1());
            while (itr.valid() && pRemovedItr.valid())
            {
                rank_type r = originalRank(*pRemovedItr);
                BOOST_ASSERT(r != *itr);
                if (r < *itr)
                {
                    b.push_back(r);
                    ++pRemovedItr;
                }
                else
                {
                    b.push_back(*itr);
                    ++itr;
                }
            }
            while (itr.valid())
            {
                b.push_back(*itr);
                ++itr;
            }
            while (pRemovedItr.valid())
            {
                rank_type r = originalRank(*pRemovedItr);
                b.push_back(r);
                ++pRemovedItr;
            }
            b.end(mArray.count());
        }
        mMask = std::unique_ptr<Mask>(new Mask("mask", mFac));
    }

    SparseArrayView(const SparseArray& pArray)
        : mArray(pArray)
    {
    }

private:

    const SparseArray& mArray;
    StringFileFactory mFac;
    std::unique_ptr<Mask> mMask;
};


#endif // SPARSEARRAYVIEW_HH
