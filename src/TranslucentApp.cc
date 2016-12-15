// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "TranslucentApp.hh"

#include "Utils.hh"
#include "GossamerException.hh"
#include "GossCmdReg.hh"
#include "GossOption.hh"
#include "GossOptionChecker.hh"
#include "Logger.hh"
#include "PhysicalFileFactory.hh"
#include "Timer.hh"
#include "TranslucentVersion.hh"

#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>

#include "GossCmdHelp.hh"
#include "GossCmdBuildGraph.hh"
#include "GossCmdLintGraph.hh"
#include "GossCmdTrimGraph.hh"
#include "GossCmdPruneTips.hh"
#include "GossCmdPopBubbles.hh"
#include "TransCmdAssemble.hh"
#include "TransCmdTrimRelative.hh"
#include "TransCmdMergeGraphWithReference.hh"

using namespace boost;
using namespace boost::program_options;
using namespace std;
using namespace Gossamer;

typedef vector<string> strings;

namespace // anonymous
{
    GossOptions globalOpts;

    GossOptions commonOpts;

    vector<GossCmdReg> cmds;
} // namespace anonymous

const char*
TranslucentApp::name() const
{
    return "translucent";
}

const char*
TranslucentApp::version() const
{
    return Gossamer::version;
}

TranslucentApp::TranslucentApp()
    : App(globalOpts, commonOpts)
{

    cmds.push_back(GossCmdReg("help", GossCmdFactoryPtr(new GossCmdFactoryHelp(*this))));
    cmds.push_back(GossCmdReg("build-graph", GossCmdFactoryPtr(new GossCmdFactoryBuildGraph())));
    cmds.push_back(GossCmdReg("lint-graph", GossCmdFactoryPtr(new GossCmdFactoryLintGraph())));
    cmds.push_back(GossCmdReg("trim-graph", GossCmdFactoryPtr(new GossCmdFactoryTrimGraph)));
    cmds.push_back(GossCmdReg("trim-relative", GossCmdFactoryPtr(new TransCmdFactoryTrimRelative)));
    cmds.push_back(GossCmdReg("prune-tips", GossCmdFactoryPtr(new GossCmdFactoryPruneTips())));
    cmds.push_back(GossCmdReg("pop-bubbles", GossCmdFactoryPtr(new GossCmdFactoryPopBubbles())));
    cmds.push_back(GossCmdReg("assemble", GossCmdFactoryPtr(new TransCmdFactoryAssemble())));
    cmds.push_back(GossCmdReg("merge-graph-with-reference", GossCmdFactoryPtr(new TransCmdFactoryMergeGraphWithReference())));

    globalOpts.addOpt<strings>("debug", "D",
                                "enable particular debugging output");
    globalOpts.addOpt<bool>("help", "h", "show a help message");
    globalOpts.addOpt<string>("log-file", "l", "place to write messages");
    globalOpts.addOpt<strings>("tmp-dir", "", "a directory to use for temporary files (default /tmp)");
    globalOpts.addOpt<uint64_t>("num-threads", "T", "maximum number of worker threads to use, where possible");
    globalOpts.addOpt<bool>("verbose", "v", "show progress messages");
    globalOpts.addOpt<bool>("version", "V", "show the software version");

    commonOpts.addOpt<uint64_t>("buffer-size", "B", "maximum size (in GB) for in-memory buffers (default: 2)");
    commonOpts.addOpt<strings>("fasta-in", "I", "input file in FASTA format");
    commonOpts.addOpt<strings>("fastas-in", "F", "input file containing filenames in FASTA format");
    commonOpts.addOpt<strings>("fastq-in", "i", "input file in FASTQ format");
    commonOpts.addOpt<strings>("fastqs-in", "f", "input file containing filenames in FASTQ format");
    commonOpts.addOpt<strings>("line-in", "", "input file with one sequence per line");
    commonOpts.addOpt<strings>("graph-in", "G", "name of the input graph object");
    commonOpts.addOpt<string>("graph-out", "O", "name of the output graph object");
    commonOpts.addOpt<uint64_t>("kmer-size", "k", "kmer size to use");
    commonOpts.addOpt<uint64_t>("cutoff", "C", "coverage cutoff");
    commonOpts.addOpt<double>("relative-cutoff", "", "relative coverage cutoff");
    commonOpts.addOpt<uint64_t>("min-length", "", "the minimum length contig to print (default 0)");
    commonOpts.addOpt<string>("output-file", "o", "output file name ('-' for standard output)");
    commonOpts.addOpt<bool>("estimate-only", "",
            "only estimate coverage - don't run command");
    commonOpts.addOpt<uint64_t>("iterate", "",
            "repeat the graph editing operation");
    commonOpts.addOpt<uint64_t>("log-hash-slots", "S",
            "log2 of the number of hash slots to use (default 24)");
}
