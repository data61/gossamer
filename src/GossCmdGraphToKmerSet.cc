// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdGraphToKmerSet.hh"

#include "Debug.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
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


void
GossCmdGraphToKmerSet::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    Timer t;

    Graph::LazyIterator itr(mIn, fac);
    const uint64_t K(itr.K());
    const uint64_t rho(K + 1);
    const uint64_t z(itr.count());
    ProgressMonitorNew mon(log, z);

    log(info, "building (k+1)-mer set");
    KmerSet::Builder bld(rho, mOut, fac, z);
    for (uint64_t x=0; itr.valid(); ++itr)
    {
        mon.tick(++x);
        Gossamer::position_type rhomer = (*itr).first.value();
        if (rhomer.isNormal(rho))
        {
            bld.push_back(rhomer);
        }
    }
    bld.end();
    
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryGraphToKmerSet::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out;
    chk.getMandatory("graph-out", out);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdGraphToKmerSet(in, out));
}

GossCmdFactoryGraphToKmerSet::GossCmdFactoryGraphToKmerSet()
    : GossCmdFactory("generate a graph's k-mer set")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
}
