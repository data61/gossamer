// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "EdgeAndCount.hh"
#include "Graph.hh"
#include "JobManager.hh"
#include "RankSelect.hh"
#include "VByteCodec.hh"

#include <iostream>
#include <stdio.h>

using namespace std;
using namespace boost;

namespace // anonymous
{
    typedef Gossamer::EdgeAndCount Item;

    class Elem
    {
    public:
        virtual uint64_t edgesRead() const = 0;

        virtual string label() const = 0;

        virtual const JobManager::Tokens& deps() const = 0;

        virtual const vector<Item>& items() const = 0;

        virtual void moveToFront(const vector<Item>::const_iterator& pFrom) = 0;

        virtual void fill() = 0;

        virtual ~Elem() {}
    };
    typedef boost::shared_ptr<Elem> ElemPtr;

    class Loader : public Elem
    {
    public:
        uint64_t edgesRead() const
        {
            return mEdgesRead;
        }

        string label() const
        {
            return mFileName;
        }

        const JobManager::Tokens& deps() const
        {
            return mDeps;
        }

        const vector<Item>& items() const
        {
            return mItems;
        }

        void moveToFront(const vector<Item>::const_iterator& pFrom)
        {
            if (pFrom == mItems.begin())
            {
                return;
            }
            //cerr << "loader: moving " << (mItems.end() - pFrom) << " items." << endl;
            vector<Item>::iterator i = mItems.begin();
            for (vector<Item>::const_iterator j = pFrom; j != mItems.end();)
            {
                *i++ = *j++;
            }
            mItems.erase(i, mItems.end());
            //cerr << "mItems.size() = " << mItems.size() << endl;
        }

        void fill()
        {
            //cerr << "filling from " << mFileName << endl;
            //cerr << "space = " << (mNumItems - mItems.size()) << endl;
            FileFactory::InHolderPtr inp(mFactory.in(mFileName));
            istream& in(**inp);
            in.seekg(mOffset);

            //cerr << "seeked = " << mOffset << endl;
            //cerr << " in.good() = " << in.good() << endl;
            //uint64_t z = mItems.size();
            //cerr << "mItem = " << lexical_cast<string>(mItem.first) << " : " << mItem.second << endl;
            while (in.good() && mItems.size() < mNumItems)
            {
                EdgeAndCountCodec::decode(in, mItem);
                if (!in.good())
                {
                    break;
                }
                BOOST_ASSERT(mItems.empty() || mItems.back() < mItem);
                mItems.push_back(mItem);
                mEdgesRead++;
            }
            //cerr << "read " << (mItems.size() - z) << endl;
            mOffset = in.tellg();
            //cerr << "mOffset = " << mOffset << endl << endl;
        }

        Loader(const string& pFileName, uint64_t pExpectedEdges, uint64_t pNumItems, FileFactory& pFactory)
            : mFactory(pFactory), mFileName(pFileName), mExpectedEdges(pExpectedEdges), mNumItems(pNumItems),
              mOffset(0), mEdgesRead(0), mItem(Gossamer::position_type(0), 0)
        {
            mItems.reserve(mNumItems);
        }

        ~Loader()
        {
            //cerr << mEdgesRead << '\t' << mExpectedEdges << endl;
            BOOST_ASSERT(mEdgesRead == mExpectedEdges);
        }

    private:
        FileFactory& mFactory;
        const string mFileName;
        const uint64_t mExpectedEdges;
        const uint64_t mNumItems;
        const JobManager::Tokens mDeps;
        uint64_t mOffset;
        uint64_t mEdgesRead;
        Item mItem;
        vector<Item> mItems;
    };

    class Merger : public Elem
    {
    public:
        uint64_t edgesRead() const
        {
            return mLhs->edgesRead() + mRhs->edgesRead();
        }

        string label() const
        {
            return "merger";
        }

        const JobManager::Tokens& deps() const
        {
            return mDeps;
        }

        const vector<Item>& items() const
        {
            return mItems;
        }

        void moveToFront(const vector<Item>::const_iterator& pFrom)
        {
            if (pFrom == mItems.begin())
            {
                return;
            }
            //cerr << "merger: moving " << (mItems.end() - pFrom) << " items." << endl;
            vector<Item>::iterator i = mItems.begin();
            for (vector<Item>::const_iterator j = pFrom; j != mItems.end();)
            {
                *i++ = *j++;
            }
            mItems.erase(i, mItems.end());
        }

        void fill()
        {
            if (mItems.size())
            {
                return;
            }

            const vector<Item>& lhs = mLhs->items();
            const vector<Item>& rhs = mRhs->items();
            //cerr << (void*)this << '\t' << "in" << '\t' << lhs.size() << '\t' << rhs.size() << '\t'
            //        << mItems.size() << endl;
            mItems.reserve(mItems.size() + lhs.size() + rhs.size());
            vector<Item>::const_iterator l = lhs.begin();
            //cerr << "lhs.size() = " << lhs.size() << endl;
            vector<Item>::const_iterator r = rhs.begin();
            //cerr << "rhs.size() = " << rhs.size() << endl;
            while (l != lhs.end() && r != rhs.end())
            {
                if (l->first < r->first)
                {
                    BOOST_ASSERT(mItems.empty() || mItems.back() < *l);
                    mItems.push_back(*l);
                    ++l;
                    continue;
                }
                if (l->first > r->first)
                {
                    BOOST_ASSERT(mItems.empty() || mItems.back() < *r);
                    mItems.push_back(*r);
                    ++r;
                    continue;
                }
                BOOST_ASSERT(mItems.empty() || mItems.back() < *l);
                mItems.push_back(*l);
                mItems.back().second += r->second;
                ++l;
                ++r;
            }
            if (lhs.empty())
            {
                while (r != rhs.end())
                {
                    mItems.push_back(*r);
                    ++r;
                }
            }
            if (rhs.empty())
            {
                while (l != lhs.end())
                {
                    mItems.push_back(*l);
                    ++l;
                }
            }

            //cerr << (void*)this << '\t' << "out" << '\t' << (lhs.end() - l) << '\t' << (rhs.end() - r) << '\t'
            //        << mItems.size() << endl;
            mDeps.clear();
            if (lhs.size() > 0)
            {
                //cerr << "merger: merged " << (l - lhs.begin()) << " of " << lhs.size() << " lhs items." << endl;
                mLhs->moveToFront(l);
                //cerr << "previously got " << lhs.size() << " items, scheduling " << mLhs->label() << endl;
                JobManager::Token lt = mMgr.enqueue(std::bind(&Elem::fill, mLhs.get()), mLhs->deps());
                mDeps.insert(lt);
            }
            if (rhs.size() > 0)
            {
                //cerr << "merger: merged " << (r - rhs.begin()) << " of " << rhs.size() << " rhs items." << endl;
                mRhs->moveToFront(r);
                //cerr << "previously got " << rhs.size() << " items, scheduling " << mRhs->label() << endl;
                JobManager::Token rt = mMgr.enqueue(std::bind(&Elem::fill, mRhs.get()), mRhs->deps());
                mDeps.insert(rt);
            }
        }

        Merger(JobManager& pMgr, const ElemPtr& pLhs, const ElemPtr& pRhs)
            : mMgr(pMgr), mLhs(pLhs), mRhs(pRhs)
        {
            JobManager::Token lt = mMgr.enqueue(std::bind(&Elem::fill, mLhs.get()), mLhs->deps());
            mDeps.insert(lt);
            JobManager::Token rt = mMgr.enqueue(std::bind(&Elem::fill, mRhs.get()), mRhs->deps());
            mDeps.insert(rt);
        }

    private:
        JobManager& mMgr;
        JobManager::Tokens mDeps;
        ElemPtr mLhs;
        ElemPtr mRhs;
        vector<Item> mItems;
    };

    ElemPtr build(const vector<string>& pFileNames, const vector<uint64_t>& pExpectedEdges,
                  FileFactory& pFactory, uint64_t pNumItems, JobManager& pMgr)
    {
        BOOST_ASSERT(pFileNames.size() > 0);
        deque<ElemPtr> ptrs;
        for (uint64_t i = 0; i < pFileNames.size(); ++i)
        {
            ptrs.push_back(ElemPtr(new Loader(pFileNames[i], pExpectedEdges[i], pNumItems, pFactory)));
        }
        while (ptrs.size() > 1)
        {
            ElemPtr l = ptrs.front();
            ptrs.pop_front();
            ElemPtr r = ptrs.front();
            ptrs.pop_front();
            ptrs.push_back(ElemPtr(new Merger(pMgr, l, r)));
            //cerr << "build: " << (void*)ptrs.back().get() << '\t' << (void*)l.get() << '\t' << (void*)r.get() << endl;
        }
        return ptrs.front();
    }

    template <typename T>
    void do_merge(JobManager& pMgr, ElemPtr& pRoot, const string& pBaseName, uint64_t pK, uint64_t pN, FileFactory& pFactory)
    {
        typename T::Builder bld(pK, pBaseName, pFactory, pN);
        while (true)
        {
            JobManager::Token t = pMgr.enqueue(std::bind(&Elem::fill, pRoot.get()), pRoot->deps());
            pMgr.wait(t);
            if (pRoot->items().empty())
            {
                break;
            }
            const vector<Item>& itms(pRoot->items());
            for (vector<Item>::const_iterator i = itms.begin(); i != itms.end(); ++i)
            {
                //cerr << lexical_cast<string>(i->first) << '\t' << i->second << endl;
                bld.push_back(i->first, i->second);
            }
            pRoot->moveToFront(itms.end());
        }
        bld.end();
    }
}
// namespace anonymous

template <typename T>
void
AsyncMerge::merge(const vector<string>& pParts, const vector<uint64_t>& pSizes, const string& pGraphName,
                  uint64_t pK, uint64_t pN, uint64_t pNumThreads, uint64_t pBufferSize, FileFactory& pFactory)
{
    {
        JobManager mgr(pNumThreads);
        ElemPtr r = build(pParts, pSizes, pFactory, pBufferSize, mgr);
        do_merge<T>(mgr, r, pGraphName, pK, pN, pFactory);
        mgr.wait();
    }
}
