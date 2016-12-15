// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossApp.hh"

#include "Debug.hh"
#include "Utils.hh"
#include "GossamerException.hh"
#include "GossCmdReg.hh"
#include "GossOption.hh"
#include "GossVersion.hh"

#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>

#include "GossCmdAnnotateKmers.hh"
#include "GossCmdBuildDb.hh"
#include "GossCmdBuildEdgeIndex.hh"
#include "GossCmdBuildEntryEdgeSet.hh"
#include "GossCmdBuildGraph.hh"
#include "GossCmdBuildKmerSet.hh"
#include "GossCmdBuildScaffold.hh"
#include "GossCmdBuildSubgraph.hh"
#include "GossCmdBuildSupergraph.hh"
#include "GossCmdClassifyReads.hh"
#include "GossCmdClipLinks.hh"
#include "GossCmdComputeNearKmers.hh"
#include "GossCmdCountComponents.hh"
#include "GossCmdDetectVariants.hh"
#include "GossCmdDotGraph.hh"
#include "GossCmdDotSupergraph.hh"
#include "GossCmdDumpGraph.hh"
#include "GossCmdDumpKmerSet.hh"
#include "GossCmdEstimateErrorRate.hh"
#include "GossCmdExtractCoreGenome.hh"
#include "GossCmdExtractReads.hh"
#include "GossCmdFilterReads.hh"
#include "GossCmdFixReads.hh"
#include "GossCmdGraphToKmerSet.hh"
#include "GossCmdGroupReads.hh"
#include "GossCmdHelp.hh"
#include "GossCmdIntersectKmerSets.hh"
#include "GossCmdLintGraph.hh"
#include "GossCmdMergeGraphs.hh"
#include "GossCmdMergeKmerSets.hh"
#include "GossCmdMergeAndAnnotateKmerSets.hh"
#include "GossCmdPoolSamples.hh"
#include "GossCmdPopBubbles.hh"
#include "GossCmdPrintContigs.hh"
#include "GossCmdPruneTips.hh"
#include "GossCmdRestoreGraph.hh"
#include "GossCmdSubtractKmerSet.hh"
#include "GossCmdThreadPairs.hh"
#include "GossCmdThreadReads.hh"
#include "GossCmdScaffold.hh"
#include "GossCmdTrimGraph.hh"
#include "GossCmdTrimPaths.hh"

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
GossApp::name() const
{
    return "goss";
}

const char*
GossApp::version() const
{
    return Gossamer::version;
}


#undef ONLY_RELEASED_COMMANDS

GossApp::GossApp()
    : App(globalOpts, commonOpts)
{
    cmds.push_back(GossCmdReg("build-entry-edge-set", GossCmdFactoryPtr(new GossCmdFactoryBuildEntryEdgeSet)));
    cmds.push_back(GossCmdReg("build-graph", GossCmdFactoryPtr(new GossCmdFactoryBuildGraph)));
    cmds.push_back(GossCmdReg("build-scaffold", GossCmdFactoryPtr(new GossCmdFactoryBuildScaffold)));
    cmds.push_back(GossCmdReg("build-supergraph", GossCmdFactoryPtr(new GossCmdFactoryBuildSupergraph)));
    cmds.push_back(GossCmdReg("dump-graph", GossCmdFactoryPtr(new GossCmdFactoryDumpGraph)));
    cmds.push_back(GossCmdReg("help", GossCmdFactoryPtr(new GossCmdFactoryHelp(*this))));
    cmds.push_back(GossCmdReg("lint-graph", GossCmdFactoryPtr(new GossCmdFactoryLintGraph)));
    cmds.push_back(GossCmdReg("merge-graphs", GossCmdFactoryPtr(new GossCmdFactoryMergeGraphs)));
    cmds.push_back(GossCmdReg("pop-bubbles", GossCmdFactoryPtr(new GossCmdFactoryPopBubbles)));
    cmds.push_back(GossCmdReg("print-contigs", GossCmdFactoryPtr(new GossCmdFactoryPrintContigs)));
    cmds.push_back(GossCmdReg("prune-tips", GossCmdFactoryPtr(new GossCmdFactoryPruneTips)));
    cmds.push_back(GossCmdReg("restore-graph", GossCmdFactoryPtr(new GossCmdFactoryRestoreGraph)));
    cmds.push_back(GossCmdReg("scaffold", GossCmdFactoryPtr(new GossCmdFactoryScaffold)));
    cmds.push_back(GossCmdReg("thread-pairs", GossCmdFactoryPtr(new GossCmdFactoryThreadPairs)));
    cmds.push_back(GossCmdReg("thread-reads", GossCmdFactoryPtr(new GossCmdFactoryThreadReads)));
    cmds.push_back(GossCmdReg("trim-graph", GossCmdFactoryPtr(new GossCmdFactoryTrimGraph)));

#ifndef ONLY_RELEASED_COMMANDS
    cmds.push_back(GossCmdReg("annotate-kmers", GossCmdFactoryPtr(new GossCmdFactoryAnnotateKmers)));
    cmds.push_back(GossCmdReg("build-db", GossCmdFactoryPtr(new GossCmdFactoryBuildDb)));
    cmds.push_back(GossCmdReg("build-edge-index", GossCmdFactoryPtr(new GossCmdFactoryBuildEdgeIndex)));
    cmds.push_back(GossCmdReg("build-kmer-set", GossCmdFactoryPtr(new GossCmdFactoryBuildKmerSet)));
    cmds.push_back(GossCmdReg("build-subgraph", GossCmdFactoryPtr(new GossCmdFactoryBuildSubgraph)));
    cmds.push_back(GossCmdReg("clip-links", GossCmdFactoryPtr(new GossCmdFactoryClipLinks)));
    cmds.push_back(GossCmdReg("compute-near-kmers", GossCmdFactoryPtr(new GossCmdFactoryComputeNearKmers)));
    cmds.push_back(GossCmdReg("count-components", GossCmdFactoryPtr(new GossCmdFactoryCountComponents)));
    cmds.push_back(GossCmdReg("detect-variants", GossCmdFactoryPtr(new GossCmdFactoryDetectVariants)));
    cmds.push_back(GossCmdReg("dot-graph", GossCmdFactoryPtr(new GossCmdFactoryDotGraph)));
    cmds.push_back(GossCmdReg("dot-supergraph", GossCmdFactoryPtr(new GossCmdFactoryDotSupergraph)));
    cmds.push_back(GossCmdReg("dump-kmer-set", GossCmdFactoryPtr(new GossCmdFactoryDumpKmerSet)));
    cmds.push_back(GossCmdReg("estimate-errors", GossCmdFactoryPtr(new GossCmdFactoryEstimateErrorRate)));
    cmds.push_back(GossCmdReg("extract-core-genome", GossCmdFactoryPtr(new GossCmdFactoryExtractCoreGenome)));
    cmds.push_back(GossCmdReg("extract-reads", GossCmdFactoryPtr(new GossCmdFactoryExtractReads)));
    cmds.push_back(GossCmdReg("filter-reads", GossCmdFactoryPtr(new GossCmdFactoryFilterReads)));
    cmds.push_back(GossCmdReg("fix-reads", GossCmdFactoryPtr(new GossCmdFactoryFixReads)));
    cmds.push_back(GossCmdReg("graph-to-kmer-set", GossCmdFactoryPtr(new GossCmdFactoryGraphToKmerSet)));
    cmds.push_back(GossCmdReg("intersect-kmer-sets", GossCmdFactoryPtr(new GossCmdFactoryIntersectKmerSets)));
    cmds.push_back(GossCmdReg("merge-kmer-sets", GossCmdFactoryPtr(new GossCmdFactoryMergeKmerSets)));
    cmds.push_back(GossCmdReg("merge-and-annotate-kmer-sets", GossCmdFactoryPtr(new GossCmdFactoryMergeAndAnnotateKmerSets)));
    cmds.push_back(GossCmdReg("pool-samples", GossCmdFactoryPtr(new GossCmdFactoryPoolSamples)));
    cmds.push_back(GossCmdReg("subtract-kmer-set", GossCmdFactoryPtr(new GossCmdFactorySubtractKmerSet)));
    cmds.push_back(GossCmdReg("trim-paths", GossCmdFactoryPtr(new GossCmdFactoryTrimPaths)));
#endif

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
    commonOpts.addOpt<string>("input-file", "f", "input file name ('-' for standard input)");
    commonOpts.addOpt<uint64_t>("kmer-size", "k", "kmer size to use");
    commonOpts.addOpt<uint64_t>("cutoff", "C", "coverage cutoff");
    commonOpts.addOpt<double>("relative-cutoff", "", "relative coverage cutoff");
    commonOpts.addOpt<string>("output-file", "o", "output file name ('-' for standard output)");
    commonOpts.addOpt<bool>("no-sequence", "", 
            "rather than produce a fasta file, produce a table containing the sequence header information");
    commonOpts.addOpt<uint64_t>("min-length", "", "the minimum length contig to print (default 0)");
    commonOpts.addOpt<uint64_t>("expected-coverage", "", "expected coverage");
    commonOpts.addOpt<uint64_t>("edge-cache-rate", "", 
            "edge cache size as a proportion of edges (default 4)");
    commonOpts.addOpt<uint64_t>("min-link-count", "",
            "discard links with lower count (default 10)");
    commonOpts.addOpt<bool>("preserve-read-sense", "",
            "preserve the sense of reads (forward vs reverse complement)");
    commonOpts.addOpt<bool>("estimate-only", "",
            "only estimate coverage - don't run command");
    commonOpts.addOpt<uint64_t>("iterate", "",
            "repeat the graph editing operation");
    commonOpts.addOpt<uint64_t>("log-hash-slots", "S",
            "log2 of the number of hash slots to use (default 24)");
    commonOpts.addOpt<bool>("paired-ends", "",
            "paired-end reads, i.e. L -> <- R (default)");
    commonOpts.addOpt<bool>("mate-pairs", "", 
            "mate-pair reads, i.e. R <- -> L");
    commonOpts.addOpt<bool>("innies", "", 
            "innie oriented reads - synonymous with --paired-end");
    commonOpts.addOpt<bool>("outies", "", 
            "outie oriented reads, i.e. L <- -> R");
    commonOpts.addOpt<uint64_t>("insert-expected-size", "",
            "expected insert size");
    commonOpts.addOpt<double>("insert-size-std-dev", "",
            "standard deviation of insert sizes as a percentage of length (default 10%)");
    commonOpts.addOpt<double>("insert-size-tolerance", "",
            "range of allowable insert sizes - in standard deviations (default 2.0)");
    commonOpts.addOpt<strings>("graphs-in", "",
            "read graph names (one per line) from the given file.");
    commonOpts.addOpt<uint64_t>("max-merge", "",
            "The maximum number of graphs to merge at once.");
    commonOpts.addOpt<bool>("delete-scaffold", "",
            "Delete any scaffold files associated with the supergraph before proceeding.");
}

