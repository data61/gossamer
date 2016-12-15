// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "ProgressMonitor.hh"
#include "RankSelect.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace // anonymous
{
    const uint64_t gMaxCount = 1ULL << 63;

    template <typename T>
    class Merge
    {
    public:
        typedef boost::shared_ptr<Merge<T> > Ptr;

        virtual bool valid() const = 0;

        virtual const pair<typename T::Edge,uint32_t>& curr() const = 0;

        virtual void next() = 0;

        virtual ~Merge() {}
    };

    template <typename T>
    class CursorMerge : public Merge<T>
    {
    public:
        virtual bool valid() const
        {
            return mItr.valid();
        }

        virtual const pair<typename T::Edge,uint32_t>& curr() const
        {
            return mCurr;
        }

        virtual void next()
        {
            BOOST_ASSERT(mItr.valid());
            ++mItr;
            if (mItr.valid())
            {
                mCurr = *mItr;
            }
        }

        CursorMerge(const string& pBaseName, FileFactory& pFactory)
            : mItr(pBaseName, pFactory), mCurr(typename T::Edge(Gossamer::position_type(0)), 0)
        {
            if (mItr.valid())
            {
                mCurr = *mItr;
            }
        }

    private:
        typename T::LazyIterator mItr;
        pair<typename T::Edge,uint32_t> mCurr;
    };

    template <typename T>
    class PairMerge : public Merge<T>
    {
    public:
        virtual bool valid() const
        {
            return mValid;
        }

        virtual const pair<typename T::Edge,uint32_t>& curr() const
        {
            return mCurr;
        }

        virtual void next()
        {
            BOOST_ASSERT(valid());
            BOOST_ASSERT(mLhs.valid() || mRhs.valid());
            if (mLeft)
            {
                mLhs.next();
            }
            else
            {
                mRhs.next();
            }

            mValid = mLhs.valid() || mRhs.valid();
            if (mLhs.valid() && mRhs.valid())
            {
                mLeft = mLhs.curr().first <= mRhs.curr().first;
            }
            else
            {
                mLeft = mLhs.valid();
            }
            if (mValid)
            {
                mCurr = (mLeft ? mLhs.curr() : mRhs.curr());
            }
        }

        PairMerge(Merge<T>& pLhs, Merge<T>& pRhs)
            : mLhs(pLhs), mRhs(pRhs), mCurr(typename T::Edge(Gossamer::position_type(0)), 0)
        {
            mValid = mLhs.valid() || mRhs.valid();
            if (mLhs.valid() && mRhs.valid())
            {
                mLeft = mLhs.curr().first <= mRhs.curr().first;
            }
            else
            {
                mLeft = mLhs.valid();
            }
            if (mValid)
            {
                mCurr = (mLeft ? mLhs.curr() : mRhs.curr());
            }
        }

    private:
        Merge<T>& mLhs;
        Merge<T>& mRhs;
        bool mValid;
        bool mLeft;
        pair<typename T::Edge,uint32_t> mCurr;
    };

} // namespace anonymous

template<typename T>
void
GossCmdMerge<T>::operator()(const GossCmdContext& pCxt)
{
    // Track whether or not the graph should be removed after use.
    typedef pair<string,bool> Item;

    FileFactory& fac(pCxt.fac);

    deque<Item> todo;
    for (uint64_t i = 0; i < mIns.size(); ++i)
    {
        todo.push_back(Item(mIns[i], false));
    }

    while (todo.size() > mMaxMerge)
    {
        vector<string> ins;
        vector<string> toRemove;
        for (uint64_t i = 0; i < mMaxMerge; ++i)
        {
            ins.push_back(todo.front().first);
            if (todo.front().second)
            {
                toRemove.push_back(todo.front().first);
            }
            todo.pop_front();
        }
        string out = fac.tmpName();
        merge(ins, out, pCxt);
        todo.push_back(Item(out,true));

        for (uint64_t i = 0; i < toRemove.size(); ++i)
        {
            T::remove(toRemove[i], fac);
        }
    }

    vector<string> ins;
    vector<string> toRemove;
    while (!todo.empty())
    {
        ins.push_back(todo.front().first);
        if (todo.front().second)
        {
            toRemove.push_back(todo.front().first);
        }
        todo.pop_front();
    }
    merge(ins, mOut, pCxt);

    for (uint64_t i = 0; i < toRemove.size(); ++i)
    {
        T::remove(toRemove[i], fac);
    }
}

template<typename T>
void
GossCmdMerge<T>::merge(const strings& pIns, const string& pOut, const GossCmdContext& pCxt)
{
    BOOST_ASSERT(pIns.size() > 0);

    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    uint64_t k = 0;
    uint64_t tot = 0;
    bool asymmetric = false;
    {
        bool first = true;
        for (uint64_t i = 0; i < pIns.size(); ++i)
        {
            typename T::LazyIterator itr(pIns[i], fac);

            if (first)
            {
                k = itr.K();
                first = false;
                asymmetric = itr.asymmetric();
            }
            else
            {
                if (itr.K() != k)
                {
                    string msg("all graphs involved in a merge must have the same kmer-size.\n"
                                 + pIns[0] + " has k=" + lexical_cast<string>(k) + ".\n"
                                 + pIns[i] + " has k=" + lexical_cast<string>(itr.K()) + ".\n");
                    BOOST_THROW_EXCEPTION(
                        Gossamer::error()
                            << Gossamer::general_error_info(msg));
                }

                if (asymmetric != itr.asymmetric())
                {
                    string msg("graphs involved in a merge must either all preserve sense or not.\n"
                                 + pIns[0] + (asymmetric ? " preserves sense" : " does not preserve sense") + ".\n"
                                 + pIns[i] + (itr.asymmetric() ? " preserves sense" : " does not preserve sense") + ".\n");
                    BOOST_THROW_EXCEPTION(
                        Gossamer::error()
                            << Gossamer::general_error_info(msg));
                }
            }

            string s(" ");
            uint64_t n = itr.count();
            log(info, s + pIns[i] + s + lexical_cast<string>(n));
            tot += n; 
        }
    }

    vector<typename Merge<T>::Ptr> mergers;
    vector<typename Merge<T>::Ptr> ms;
    for (uint64_t i = 0; i < pIns.size(); ++i)
    {
        typename Merge<T>::Ptr m(new CursorMerge<T>(pIns[i], fac));
        if (m->valid())
        {
            ms.push_back(m);
        }
    }
    while (ms.size() > 1)
    {
        mergers.insert(mergers.end(), ms.begin(), ms.end());
        vector<typename Merge<T>::Ptr> tmp;
        typename vector<typename Merge<T>::Ptr>::iterator i = ms.begin();
        while (i != ms.end())
        {
            typename Merge<T>::Ptr m0 = *i;
            ++i;
            if (i == ms.end())
            {
                tmp.push_back(m0);
                break;
            }
            typename Merge<T>::Ptr m1 = *i;
            ++i;
            tmp.push_back(typename Merge<T>::Ptr(new PairMerge<T>(*m0, *m1)));
        }
        ms.swap(tmp);
    }
    Merge<T>& m(*ms.front());

    log(info, "starting graph merge");

    typename T::Builder dest(k, pOut, fac, tot, asymmetric);

    Timer t;
    pair<typename T::Edge,uint64_t> p(typename T::Edge(~Gossamer::position_type(0)), 0);
    uint64_t rt = 0;
    ProgressMonitorNew mon(log, tot);
    while (m.valid())
    {
        if (p.first != m.curr().first)
        {
            if (p.second > 0)
            {
                dest.push_back(p.first.value(), min(p.second, gMaxCount));
            }
            p = m.curr();
        }
        else
        {
            p.second += m.curr().second;
        }
        m.next();
        mon.tick(++rt);
    }
    if (p.second > 0)
    {
        dest.push_back(p.first.value(), min(p.second, gMaxCount));
    }
    dest.end();
    log(info, "finishing graph merge");
    log(info, "total build time: " + lexical_cast<string>(t.check()));
}


template<typename T>
GossCmdPtr
GossCmdFactoryMerge<T>::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    strings ins;
    chk.getRepeating0("graph-in", ins);

    string inFileName;
    chk.getOptional("graphs-in", inFileName);
    if (inFileName.size())
    {
        FileFactory& fac = pApp.fileFactory();
        FileFactory::InHolderPtr inp = fac.in(inFileName);
        istream& in(**inp);
        while (in.good())
        {
            string l;
            getline(in, l);
            if (!in.good())
            {
                break;
            }
            ins.push_back(l);
        }
    }

    if (ins.size() == 0)
    {
        chk.addError("At least one input graph must be supplied either using --graph-in or --graphs-in.\n");
    }

    uint64_t maxMerge = 8;
    chk.getOptional("max-merge", maxMerge);

    string out;
    FileFactory& fac(pApp.fileFactory());
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdMerge<T>(ins, maxMerge, out));
}

template<typename T>
GossCmdFactoryMerge<T>::GossCmdFactoryMerge()
    : GossCmdFactory("create a new graph by merging zero or more existing graphs")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("graphs-in");
    mCommonOptions.insert("max-merge");
}
