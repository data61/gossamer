# Makefile for building on OS X with MacPorts installed. 
# The statically linked 'goss' binary (and compiler detritus) are left in the current directory.
# Change MACPORTS_DIR to point to the base of your MacPorts installation.

CXX = g++ 
MACPORTS_DIR = /opt/local
CXXFLAGS = -I$(MACPORTS_DIR)/include -O3 -msse4.2
LDFLAGS = -L$(MACPORTS_DIR)/lib -lz -l bz2 -lboost_iostreams-mt -lboost_program_options-mt -lboost_thread-mt -lboost_system-mt -lboost_filesystem-mt -lsqlite3

.PHONY : all clean

%.d: %.cc
	$(CXX) -M $(CXXFLAGS) $< | sed 's/$*.o/& $@/g' > $@

%.o: %.cc
	echo "compiling $*.cc"; \
	$(CXX) $(CXXFLAGS) -c $*.cc

SRCS = \
    GossApp.cc \
    GossOption.cc \
    GossCmdReg.cc \
    GossCmdHelp.cc \
    GossCmdAnnotateKmers.cc \
    GossCmdBuildDb.cc \
    GossCmdBuildEdgeIndex.cc \
    GossCmdBuildEntryEdgeSet.cc \
    GossCmdBuildGraph.cc \
    GossCmdBuildKmerSet.cc \
    GossCmdBuildScaffold.cc \
    GossCmdBuildSubgraph.cc \
    GossCmdBuildSupergraph.cc \
    GossCmdClassifyReads.cc \
    GossCmdClipLinks.cc \
    GossCmdComputeNearKmers.cc \
    GossCmdCountComponents.cc \
    GossCmdDotGraph.cc \
    GossCmdDotSupergraph.cc \
    GossCmdDumpGraph.cc \
    GossCmdExtractReads.cc \
    GossCmdFilterReads.cc \
    GossCmdGroupReads.cc \
    GossCmdLintGraph.cc \
    GossCmdMergeAndAnnotateKmerSets.cc \
    GossCmdMergeGraphs.cc \
    GossCmdPopBubbles.cc \
    GossCmdPrintContigs.cc \
    GossCmdPruneTips.cc \
    GossCmdRestoreGraph.cc \
    GossCmdScaffold.cc \
    GossCmdThreadPairs.cc \
    GossCmdThreadReads.cc \
    GossCmdTrimGraph.cc \
    GossCmdTrimPaths.cc \
    GossCmdUpgradeGraph.cc \
    AnnotTree.cc \
    App.cc \
    BackyardHash.cc \
    BzipFilter.cc \
    CompactDynamicBitVector.cc \
    Utils.cc \
    Debug.cc \
    DenseArray.cc \
    BigInteger.cc \
    EdgeIndex.cc \
    EntryEdgeSet.cc \
    EstimateGraphStatistics.cc \
    ExternalSortUtils.cc \
    FileFactory.cc \
    GossamerException.cc \
    GossFastaReadBaseString.cc \
    GossFastqReadBaseString.cc \
    GossReadBaseString.cc \
    GossReadProcessor.cc \
    Graph.cc \
    GraphTrimmer.cc \
    IntegerArray.cc \
    LevenbergMarquardt.cc \
    MachDep.cc \
    MultithreadedBatchTask.cc \
    Phylogeny.cc \
    PhysicalFileFactory.cc \
    Profile.cc \
    ResolveTranscripts.cc \
    RRRArray.cc \
    ScaffoldGraph.cc \
    SmallBaseVector.cc \
    SparseArray.cc \
    StringFileFactory.cc \
    SuperGraph.cc \
    TourBus.cc \
    VariableByteArray.cc \
    VariableWidthBitArray.cc \
    WordyBitVector.cc \
    goss.cc


OBJS = $(SRCS:.cc=.o)
DEPS = $(SRCS:.cc=.d)

all: goss

goss: $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o goss

clean:
	rm -f $(OBJS) $(DEPS) goss

-include $(DEPS)

################################################################################

#       testBigInteger.cc \
#       testBitVecSet.cc \
#       testCompactDynamicBitVector.cc \
#       testDenseArray.cc \
#       testEnumerativeCode.cc \
#       testExternalVarPushSorter.cc \
#       testFixedWidthBitArray.cc \
#       testGammaCodec.cc \
#       testGeneralKmerExtractor.cc \
#       testGossCmdBuildGraph.cc \
#       testGossCmdPrintContigs.cc \
#       testGossCmdThreadReads.cc \
#       testGraph.cc \
#       testPhysicalFileFactory.cc \
#       testRRRArray.cc \
#       testRunLengthCodedBitVectorWord.cc \
#       testSmallBaseVector.cc \
#       testSparseArray.cc \
#       testTourBus.cc \
#       testUtils.cc \
#       testVariableByteArray.cc \
#       testVariableWidthBitArray.cc \
#       testWordyBitVector.cc 
