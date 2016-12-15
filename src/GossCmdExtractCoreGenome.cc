// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdExtractCoreGenome.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "Heap.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace // anonymous
{

struct Cursor
{
    std::shared_ptr<Graph::LazyIterator> itm;

    bool operator<(const Cursor& pRhs) const
    {
        BOOST_ASSERT(itm->valid());
        BOOST_ASSERT(pRhs.itm->valid());
        return *itm < *pRhs.itm;
    }

    Cursor(const string& pBasename, FileFactory& pFactory)
        : itm(new Graph::LazyIterator(pBasename, pFactory))
    {
    }
};


double sqr(const double pX)
{
    return pX * pX;
}

} // namespace anonymous

void
GossCmdExtractCoreGenome::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    Timer t;

    log(info, "counting kmer instances");
    vector<double> totals;
    for (uint64_t i = 0; i < mSrcs.size(); ++i)
    {
        uint64_t lCount = 0;
        {
            Graph::LazyIterator itr(mSrcs[i], fac);
            while (itr.valid())
            {
                lCount += (*itr).second;
                ++itr;
            }
        }
        totals.push_back(lCount);
    }

    log(info, "computing distances");
    for (uint64_t i = 0; i < mSrcs.size(); ++i)
    {
        log(info, mSrcs[i] + "...");
        for (uint64_t j = i + 1; j < mSrcs.size(); ++j)
        {
            Graph::LazyIterator lhs(mSrcs[i], fac);
            Graph::LazyIterator rhs(mSrcs[j], fac);
            double d2 = 0;
            while (lhs.valid() && rhs.valid())
            {
                if ((*lhs).first < (*rhs).first)
                {
                    d2 = sqr((*lhs).second/totals[i]);
                    ++lhs;
                    continue;
                }
                if ((*lhs).first > (*rhs).first)
                {
                    d2 = sqr((*rhs).second/totals[j]);
                    ++rhs;
                    continue;
                }
                d2 = sqr((*lhs).second/totals[i] - (*rhs).second/totals[j]);
                ++lhs;
                ++rhs;
            }
            while (lhs.valid())
            {
                d2 = sqr((*lhs).second/totals[i]);
                ++lhs;
            }
            while (rhs.valid())
            {
                d2 = sqr((*rhs).second/totals[j]);
                ++rhs;
            }
            cout << mSrcs[i] << '\t' << mSrcs[j] << '\t' << d2 << endl;
        }
    }

#if 0
    Heap<Cursor> itrs;

    log(info, "Pass 1: counting core edges");

    uint64_t tot = 0;
    for (uint64_t i = 0; i < mSrcs.size(); ++i)
    {
        Cursor itr(mSrcs[i], fac);
        if (itr.itm->valid())
        {
            tot += itr.itm->count();
            itrs.push_back(itr);
        }
    }
    itrs.heapify();

    uint64_t N = 0;
    uint64_t n = 0;
    uint64_t j = 0;
    uint64_t rt = 0;

    while (itrs.size())
    {
        ++j;
        ++rt;
        Graph::Edge e = (**itrs.front().itm).first;
        ++(*itrs.front().itm);
        if (itrs.front().itm->valid())
        {
            itrs.front_changed();
        }
        else
        {
            itrs.pop();
        }
        uint64_t c = 1;
        while (itrs.size() && (**itrs.front().itm).first == e)
        {
            ++rt;
            c++;
            ++(*itrs.front().itm);
            if (itrs.front().itm->valid())
            {
                itrs.front_changed();
            }
            else
            {
                itrs.pop();
            }
        }
        if (c >= N)
        {
            ++n;
        }
        if ((rt % (1ULL << 14)) == 0)
        {
            cerr << (100.0 * rt / tot) << endl;
        }
    }

    cerr << n << " out of " << j << " edges are common.\n";
    log(info, "Pass 2: constructing core graph");

    for (uint64_t i = 0; i < mSrcs.size(); ++i)
    {
        Cursor itr(mSrcs[i], fac);
        if (itr.itm->valid())
        {
            tot += itr.itm->count();
            itrs.push_back(itr);
        }
    }
    itrs.heapify();

    rt = 0;

    Graph::Builder bld(itrs[0].itm->K(), mDest, fac, n);
    while (itrs.size())
    {
        ++rt;
        Graph::Edge e = (**itrs.front().itm).first;
        ++(*itrs.front().itm);
        if (itrs.front().itm->valid())
        {
            itrs.front_changed();
        }
        else
        {
            itrs.pop();
        }
        uint64_t c = 1;
        while (itrs.size() && (**itrs.front().itm).first == e)
        {
            ++rt;
            c++;
            ++(*itrs.front().itm);
            if (itrs.front().itm->valid())
            {
                itrs.front_changed();
            }
            else
            {
                itrs.pop();
            }
        }
        if (c >= N)
        {
            bld.push_back(e.value(), c);
        }
        if ((rt % (1ULL << 14)) == 0)
        {
            cerr << (100.0 * rt / tot) << endl;
        }
    }
    bld.end();
#endif

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryExtractCoreGenome::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    strings srcs;
    chk.getRepeating0("graph-in", srcs);

    string dest;
    chk.getMandatory("graph-out", dest);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdExtractCoreGenome(srcs, dest));
}

GossCmdFactoryExtractCoreGenome::GossCmdFactoryExtractCoreGenome()
    : GossCmdFactory("Decorate a graph with an assignment of kmers to graphs.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
}
