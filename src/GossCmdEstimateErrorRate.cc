// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdEstimateErrorRate.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace // anonymous
{
} // namespace anonymous

void
GossCmdEstimateErrorRate::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    Timer t;

    map<uint64_t,uint64_t> h = Graph::hist(mRef, fac);
    vector<int64_t> v;
    uint64_t tot = 0;
    for (uint64_t i = 1; i < h.rbegin()->first; ++i)
    {
        map<uint64_t,uint64_t>::const_iterator f = h.find(i);
        uint64_t x = 0;
        if (f != h.end())
        {
            x = f->second;
            tot += f->first * f->second;
        }
        v.push_back(x);
    }
    uint64_t iMin = 0;
    uint64_t s = v[0];
    for (uint64_t i = 1; i + 1 < v.size(); ++i)
    {
        int64_t w = v[i + 1] - v[i - 1];
        int64_t ww = v[i - 1] - 2 * v[i] + v[i + 1];
        //cerr << (i + 1) << '\t' << w << '\t' << ww << endl;
        if (w >= 0)
        {
            break;
        }
        s += (i + 1) * v[i];
        iMin = (i + 1);
    }
    cout << mRef << '\t' << iMin << '\t' << (static_cast<double>(s)/tot) << endl;

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryEstimateErrorRate::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string ref;
    chk.getRepeatingOnce("graph-in", ref);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdEstimateErrorRate(ref));
}

GossCmdFactoryEstimateErrorRate::GossCmdFactoryEstimateErrorRate()
    : GossCmdFactory("Decorate a graph with an assignment of kmers to graphs.")
{
    mCommonOptions.insert("graph-in");
}
