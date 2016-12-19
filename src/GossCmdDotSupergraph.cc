// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdDotSupergraph.hh"

#include "LineSource.hh"
#include "EntryEdgeSet.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "ProgressMonitor.hh"
#include "RankSelect.hh"
#include "StringFileFactory.hh"
#include "SuperGraph.hh"
#include "Timer.hh"

#include <cmath>
#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;
typedef set<Graph::Edge> Edges;

static void
writeNode(ostream& pOut, bool pCore, const GossCmdDotSupergraph::Style& pStyle, 
          const string& pNodeL)
{
    const string& colour(pCore ? pStyle.mCoreColour : pStyle.mFringeColour);
    const string& shape(pStyle.mPointNodes ? "point" : "box");
    pOut << '\t' << pNodeL 
         << " ["
            << "shape=\"" << shape << "\", "
            << "label=\"" << (pStyle.mLabelNodes ? " " + pNodeL : "") << "\" "
            << "fontcolor=\"" << colour << "\""
            << "color=\"" << colour << "\""
         << "];" << endl;
}

static void
writeEdge(ostream& pOut, bool pCore, const GossCmdDotSupergraph::Style& pStyle, 
          const string& pFromL, const string& pToL, const string& pEdgeL)
{
    string l;
    if (pStyle.mLabelEdges)
    {
        l += " " + pEdgeL;
    }

    const string& colour(pCore ? pStyle.mCoreColour : pStyle.mFringeColour);
    pOut << '\t' << pFromL << " -> " << pToL
         << " ["
            << "label=\"" << l << "\" "
            << "fontcolor=\"" << colour << "\""
            << "color=\"" << colour << "\""
         << "];" << endl;
}

void
GossCmdDotSupergraph::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    FileFactory::OutHolderPtr outPtr(fac.out(mOut));
    ostream& out(**outPtr);

    auto sgPtr = SuperGraph::read(mIn, fac);
    SuperGraph& sg(*sgPtr);

    vector<SuperGraph::Node> nodes;
    sg.nodes(nodes);

    out << "digraph {" << endl;
    uint64_t k(sg.entries().K());
    uint64_t n(0);
    ProgressMonitor mon(log, nodes.size(), 100);
    SuperGraph::SuperPathIds succIds;
    dynamic_bitset<> seen(sg.size());
    for (vector<SuperGraph::Node>::const_iterator
         i = nodes.begin(); i != nodes.end(); ++i, ++n)
    {
        mon.tick(n);
        succIds.clear();
        sg.successors(*i, succIds);
        for (SuperGraph::SuperPathIds::const_iterator
             j = succIds.begin(); j != succIds.end(); ++j)
        {
            if (seen[(*j).value()])
            {
             //   continue;
            }

            const SuperPath& path(sg[*j]);
            seen[(*j).value()] = true;
            seen[path.reverseComplement().value()] = true;

            string edgeL(lexical_cast<string>((*j).value()));
            BOOST_ASSERT((*i) == path.start(sg.entries()));
            string fromL(kmerToString(k, (*i).value()));
            string toL(kmerToString(k, sg.end(path).value()));
            // bool u = sg.unique(path, 40);
            bool u = true;
            writeNode(out, u, mStyle, fromL);
            writeNode(out, u, mStyle, toL);
            writeEdge(out, u, mStyle, fromL, toL, edgeL);
        }
    }
    out << '}' << endl;
}


GossCmdPtr
GossCmdFactoryDotSupergraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string out = "-";
    chk.getOptional("output-file", out, GossOptionChecker::FileCreateCheck(fac, false));

    GossCmdDotSupergraph::Style style;
    style.mLabelNodes = false;
    style.mLabelEdges = true;
    style.mPointNodes = true;

    style.mCoreColour = "black";
    style.mFringeColour = "grey";

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdDotSupergraph(in, out, style));
}

GossCmdFactoryDotSupergraph::GossCmdFactoryDotSupergraph()
    : GossCmdFactory("write out the supergraph in dot format.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("output-file");
}
