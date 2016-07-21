#include "GossCmdBuildSupergraph.hh"

#include "LineSource.hh"
#include "Debug.hh"
#include "EdgeIndex.hh"
#include "EntryEdgeSet.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "ScaffoldGraph.hh"
#include "SuperGraph.hh"
#include "SuperPathId.hh"
#include "Timer.hh"

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

void
GossCmdBuildSupergraph::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    Timer t;

    if (ScaffoldGraph::existScafFiles(pCxt, mIn))
    {
        if (!mRemScaf)
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::usage_info("Cannot execute command: there are scaffold files associated with " 
                                        + mIn + ". Rerun with --delete-scaffold to remove these files and proceed.\n"));
        }
        ScaffoldGraph::removeScafFiles(pCxt, mIn);
    }

    LOG(log, info) << "constructing supergraph";
    auto_ptr<SuperGraph> sgPtr(SuperGraph::create(mIn, fac));
    LOG(log, info) << "writing supergraph";
    sgPtr->write(mIn, fac);
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryBuildSupergraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    bool del;
    chk.getOptional("delete-scaffold", del);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildSupergraph(in, del));
}

GossCmdFactoryBuildSupergraph::GossCmdFactoryBuildSupergraph()
    : GossCmdFactory("generate a de Bruijn graph's supergraph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("delete-scaffold");
}
