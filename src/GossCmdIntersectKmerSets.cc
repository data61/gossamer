// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdIntersectKmerSets.hh"

#include "Debug.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "KmerSet.hh"
#include "Timer.hh"

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace {

    template<typename V>
    void intersection(const GossCmdContext& pCxt, const strings& pIns, V& pVis)
    {
        // Logger& log(pCxt.log);
        FileFactory& fac(pCxt.fac);

        vector<std::shared_ptr<KmerSet::LazyIterator> > itrs;
        for (uint64_t i = 0; i < pIns.size(); ++i)
        {
            std::shared_ptr<KmerSet::LazyIterator> itr(new KmerSet::LazyIterator(pIns[i], fac));
            if (itr->valid())
            {
                itrs.push_back(itr);
            }
        }

        bool done = false;
        Gossamer::position_type targetKmer;
        while (!done)
        {
            uint64_t i = 0;
            for (; i < itrs.size(); )
            {
                KmerSet::LazyIterator& itr(*itrs[i]);
                while (itr.valid() && (*itr).first.value() < targetKmer)
                {
                    ++itr;
                }

                if (!itr.valid())
                {
                    done = true;
                    break;
                }

                if ((*itr).first.value() == targetKmer)
                {
                    ++i;
                    continue;
                }

                targetKmer = (*itr).first.value();
                i = 0;
            }
            if (i >= itrs.size())
            {
                // targetKmer is in all sets
                pVis.push_back(targetKmer);
                ++(*itrs[0]);
            }
        }
    }
    
    struct Counter
    {
        void push_back(const Gossamer::position_type& pKmer)
        {
            ++mCount;
        }

        Counter()
            : mCount(0)
        {
        }

        uint64_t mCount;
    };
}

void
GossCmdIntersectKmerSets::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    Timer t;

    if (mIns.empty())
    {
        // Don't build an empty k-mer set - we don't know k!
        log(info, "no input k-mer sets!");
        log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        return;
    }

    uint64_t K;
    {
        KmerSet::LazyIterator itr(mIns[0], fac);
        K = itr.K();
    }

    log(info, "counting k-mers");
    Counter counter;
    intersection(pCxt, mIns, counter);
    log(info, "found " + lexical_cast<string>(counter.mCount) + " k-mers");
    log(info, "building intersection");

    KmerSet::Builder builder(K, mOut, fac, counter.mCount);
    intersection(pCxt, mIns, builder);
    builder.end();
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryIntersectKmerSets::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());

    strings ins;
    chk.getOptional("graph-in", ins);

    strings inFiles;
    chk.getOptional("graphs-in", inFiles);
    chk.expandFilenames(inFiles, ins, fac);

    string out;
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdIntersectKmerSets(ins, out));
}

GossCmdFactoryIntersectKmerSets::GossCmdFactoryIntersectKmerSets()
    : GossCmdFactory("generate the intersection of the given k-mer sets")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graphs-in");
    mCommonOptions.insert("graph-out");
}
