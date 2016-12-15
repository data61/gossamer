// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildEntryEdgeSet.hh"

#include "BackgroundBlockConsumer.hh"
#include "BackgroundSafeBlockConsumer.hh"
#include "BackgroundMultiConsumer.hh"
#include "LineSource.hh"
#include "Debug.hh"
#include "DeltaCodec.hh"
#include "EntryEdgeSet.hh"
#include "EntrySetPacket.hh"
#include "EntrySets.hh"
#include "ExternalVarPushSorter.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "LineParser.hh"
#include "PacketSorter.hh"
#include "ProgressMonitor.hh"
#include "ReadSequenceFileSequence.hh"
#include "RunLengthCodedBitVectorWord.hh"
#include "RunLengthCodedSet.hh"
#include "Timer.hh"
#include "VByteCodec.hh"

#include <iostream>
#include <boost/unordered_map.hpp>
#include <boost/dynamic_bitset.hpp>

using namespace boost;
using namespace std;

typedef vector<string> strings;
typedef pair<uint64_t,Gossamer::position_type> KmerItem;
typedef vector<KmerItem> KmerItems;

static Debug dumpEntrySets("dump-entry-sets", "print the entry sets");

void
GossCmdBuildEntryEdgeSet::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);
    Timer t;
    
    log(info, "Loading graph");
    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }
    EntryEdgeSet::build(g, mIn + "-entries", fac, log, mThreads);
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}

GossCmdPtr
GossCmdFactoryBuildEntryEdgeSet::create(App& pApp, const boost::program_options::variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    uint64_t t = 4;
    chk.getOptional("num-threads", t);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildEntryEdgeSet(in, t));
}

GossCmdFactoryBuildEntryEdgeSet::GossCmdFactoryBuildEntryEdgeSet()
    : GossCmdFactory("build an entry edge set for a graph")
{
    mCommonOptions.insert("graph-in");
}
