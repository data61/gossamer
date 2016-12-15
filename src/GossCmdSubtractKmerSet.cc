// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdSubtractKmerSet.hh"

#include "Debug.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "KmerSet.hh"
#include "ProgressMonitor.hh"
#include "Timer.hh"

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace {

}

void
GossCmdSubtractKmerSet::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    Timer t;

    uint64_t n = 0;
    uint64_t orig;
    uint64_t remd = 0;
    dynamic_bitset<> rem; 
    uint64_t K;
    log(info, "calculating difference");
    {
        KmerSet::LazyIterator itr0(mIns[0], fac);
        KmerSet::LazyIterator itr1(mIns[1], fac);
        orig = itr0.count();
        rem.resize(orig);
        K = itr0.K();
        ProgressMonitorNew mon(log, orig);

        for (; itr0.valid(); ++itr0, mon.tick(++n))
        {
            const Gossamer::position_type targetKmer = (*itr0).first.value();
            while (itr1.valid() && (*itr1).first.value() < targetKmer)
            {
                ++itr1;
            }
            if (!itr1.valid())
            {
                break;
            }
            if ((*itr1).first.value() == targetKmer)
            {
                rem[n] = true;
                ++remd;
            }
        }
    }

    log(info, "found " + lexical_cast<string>(orig - remd) + " k-mers in difference");
    log(info, "building difference set");
    KmerSet::Builder builder(K, mOut, fac, orig - remd);
    uint64_t i = 0;
    for (KmerSet::LazyIterator itr(mIns[0], fac); itr.valid(); ++itr, ++i)
    {
        if (!rem[i])
        {
            builder.push_back((*itr).first.value());
        }
    }
    builder.end();
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactorySubtractKmerSet::create(App& pApp, const variables_map& pOpts)
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

    if (ins.size() != 2)
    {
        chk.addError("Exactly two input k-mer sets required!");
    }

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdSubtractKmerSet(ins, out));
}

GossCmdFactorySubtractKmerSet::GossCmdFactorySubtractKmerSet()
    : GossCmdFactory("subtract the second k-mer set from the first")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graphs-in");
    mCommonOptions.insert("graph-out");
}
