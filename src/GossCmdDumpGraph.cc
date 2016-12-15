// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdDumpGraph.hh"

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
GossCmdDumpGraph::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    // Logger& log(pCxt.log);

    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);

    FileFactory::OutHolderPtr outPtr(fac.out(mOut));
    ostream& out(**outPtr);

    uint64_t flags = 0ull;

    if (g.asymmetric())
    {
        flags |= (1ull << Graph::Header::fAsymmetric);
    }

    out << '#' << Graph::version << endl;
    out << g.K() << '\t' << g.count() << '\t' << flags << endl;

    SmallBaseVector v;
    for (Graph::Iterator itr(g); itr.valid(); ++itr)
    {
        Graph::Edge e = (*itr).first;
        uint64_t c = (*itr).second;
        v.clear();
        g.seq(e, v);
        out << v << '\t' << c << endl;
    }
}


GossCmdPtr
GossCmdFactoryDumpGraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out = "-";
    chk.getOptional("output-file", out, GossOptionChecker::FileCreateCheck(fac, false));

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdDumpGraph(in, out));
}

GossCmdFactoryDumpGraph::GossCmdFactoryDumpGraph()
    : GossCmdFactory("write out the graph in a robust text representation.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("output-file");
}
