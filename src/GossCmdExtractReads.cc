// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdExtractReads.hh"

#include "BackgroundBlockConsumer.hh"
#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "BackgroundSafeBlockConsumer.hh"
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
#include "Graph.hh"
#include "LineParser.hh"
#include "PacketSorter.hh"
#include "ProgressMonitor.hh"
#include "ReadSequenceFileSequence.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;


void
GossCmdExtractReads::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }
    const uint64_t rho = g.K() + 1;

    FileFactory::OutHolderPtr outPtr(fac.out(mOut));
    ostream& out(**outPtr);

    std::deque<GossReadSequence::Item> items;

    {
        GossReadSequenceFactoryPtr seqFac
            = std::make_shared<GossReadSequenceBasesFactory>();

        GossReadParserFactory lineParserFac(LineParser::create);
        for (auto& f: mLines)
        {
            items.push_back(GossReadSequence::Item(f,
                            lineParserFac, seqFac));
        }

        GossReadParserFactory fastaParserFac(FastaParser::create);
        for (auto& f: mFastas)
        {
            items.push_back(GossReadSequence::Item(f,
                            fastaParserFac, seqFac));
        }

        GossReadParserFactory fastqParserFac(FastqParser::create);
        for (auto& f: mFastqs)
        {
            items.push_back(GossReadSequence::Item(f,
                            fastqParserFac, seqFac));
        }
    }
    
    UnboundedProgressMonitor umon(log, 100000, " reads");
    uint64_t n = 0;
    uint64_t m = 0;
    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    for (ReadSequenceFileSequence reads(items, fac, lineSrcFac, &umon);
        reads.valid(); ++reads, ++n)
    {
        const GossRead& r = *reads;
        bool matches = false;
        for (GossRead::Iterator i(r, rho); !matches && i.valid() ;++i)
        {
            matches = g.access(Graph::Edge(i.kmer()));
        }
        if (matches)
        {
            r.print(out);
            ++m;
        }
    }
    log(info, "extracted " + lexical_cast<string>(m) + " reads, out of " + lexical_cast<string>(n));
}


GossCmdPtr
GossCmdFactoryExtractReads::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileCreateCheck createChk(fac, false);
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    strings fastas;
    chk.getRepeating0("fasta-in", fastas, readChk);

    strings fastqs;
    chk.getRepeating0("fastq-in", fastqs, readChk);

    strings lines;
    chk.getRepeating0("line-in", lines, readChk);

    string out = "-";
    chk.getOptional("output-file", out, createChk);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdExtractReads(in, fastas, fastqs, lines, out));
}

GossCmdFactoryExtractReads::GossCmdFactoryExtractReads()
    : GossCmdFactory("extract reads which map on to a graph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("output-file");
}
