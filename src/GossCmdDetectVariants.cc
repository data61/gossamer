// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdDetectVariants.hh"

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
GossCmdDetectVariants::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    Timer t;

    GraphPtr gPtr = Graph::open(mRef, fac);
    Graph& g(*gPtr);

    GraphPtr hPtr = Graph::open(mTarget, fac);
    Graph& h(*hPtr);

    for (uint64_t i = 0; i < h.count(); ++i)
    {
        Graph::Edge e = h.select(i);
        if (g.access(e))
        {
            continue;
        }
        Graph::Node n = h.from(e);
        pair<uint64_t,uint64_t> r = g.beginEndRank(n);
        if (r.second - r.first > 0)
        {
            SmallBaseVector v;
            g.seq(e, v);
            cout << v << '\t' << h.multiplicity(i) << endl;
        }
    }

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryDetectVariants::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string ref;
    string target;
    chk.getRepeatingTwice("graph-in", ref, target);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdDetectVariants(ref, target));
}

GossCmdFactoryDetectVariants::GossCmdFactoryDetectVariants()
    : GossCmdFactory("Decorate a graph with an assignment of kmers to graphs.")
{
    mCommonOptions.insert("graph-in");
}
