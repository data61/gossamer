
ADD_LIBRARY(gosslib
	App.cc
	AnnotTree.cc
	#AsyncMerge.cc
	BackyardHash.cc
	CompactDynamicBitVector.cc
	Debug.cc
	DenseArray.cc
	BigInteger.cc
	EdgeIndex.cc
	EntryEdgeSet.cc
	EstimateGraphStatistics.cc
	ExternalSortUtils.cc
	FileFactory.cc
	GossamerException.cc
	GossFastaReadBaseString.cc
	GossFastqReadBaseString.cc
	GossReadBaseString.cc
	GossReadProcessor.cc
	Graph.cc
	GraphTrimmer.cc
	IntegerArray.cc
	KmerSet.cc
	LevenbergMarquardt.cc
	LineSource.cc
	MachDep.cc
	MultithreadedBatchTask.cc
	Phylogeny.cc
	PhysicalFileFactory.cc
	Profile.cc
	RRRArray.cc
	ScaffoldGraph.cc
	SmallBaseVector.cc
	SparseArray.cc
	StringFileFactory.cc
	SuperGraph.cc
	TourBus.cc
	Utils.cc
	VariableByteArray.cc
	VariableWidthBitArray.cc
	WordyBitVector.cc
)

TARGET_LINK_LIBRARIES(gosslib
	${Boost_SYSTEM_LIBRARY}
	${Boost_IOSTREAMS_LIBRARY}
	${Boost_FILESYSTEM_LIBRARY}
	${ZLIB_LIBRARIES}
	${BZIP2_LIBRARIES}
	${SQLITE_LIBRARIES}
)


ADD_LIBRARY(gossapp
	GossApp.cc GossOption.cc GossCmdReg.cc GossCmdHelp.cc 
	GossCmdAnnotateKmers.cc
	GossCmdBuildDb.cc
	GossCmdBuildEdgeIndex.cc
	GossCmdBuildEntryEdgeSet.cc
	GossCmdEstimateErrorRate.cc
	GossCmdExtractCoreGenome.cc
	GossCmdBuildGraph.cc
	GossCmdBuildKmerSet.cc
	GossCmdBuildScaffold.cc
	GossCmdBuildSubgraph.cc
	GossCmdBuildSupergraph.cc
	GossCmdClassifyReads.cc
	GossCmdClipLinks.cc
	GossCmdComputeNearKmers.cc
	GossCmdCountComponents.cc
	GossCmdDetectVariants.cc
	GossCmdDotGraph.cc
	GossCmdDotSupergraph.cc
	GossCmdDumpGraph.cc
	GossCmdDumpKmerSet.cc
	GossCmdExtractReads.cc
	GossCmdFilterReads.cc
	GossCmdFixReads.cc
	GossCmdGraphToKmerSet.cc
	GossCmdGroupReads.cc
	GossCmdIntersectKmerSets.cc
	GossCmdLintGraph.cc
	GossCmdMergeAndAnnotateKmerSets.cc
	GossCmdPoolSamples.cc
	GossCmdPopBubbles.cc
	GossCmdPrintContigs.cc
	GossCmdPruneTips.cc
	GossCmdRestoreGraph.cc
	GossCmdScaffold.cc
	GossCmdSubtractKmerSet.cc
	GossCmdThreadPairs.cc
	GossCmdThreadReads.cc
	GossCmdTrimGraph.cc
	GossCmdTrimPaths.cc
	#GossCmdUpgradeGraph.cc
)


# goss

TARGET_LINK_LIBRARIES(gossapp gosslib ${Boost_PROGRAM_OPTIONS_LIBRARY})

ADD_EXECUTABLE(goss goss.cc)

TARGET_LINK_LIBRARIES(goss gossapp)


# xenome

ADD_LIBRARY(xenomeapp
	XenoApp.cc GossOption.cc GossCmdReg.cc GossCmdHelp.cc 
	GossCmdBuildKmerSet.cc
	GossCmdMergeAndAnnotateKmerSets.cc
	GossCmdComputeNearKmers.cc
	GossCmdGroupReads.cc
)

TARGET_LINK_LIBRARIES(xenomeapp gosslib ${Boost_PROGRAM_OPTIONS_LIBRARY})

ADD_EXECUTABLE(xenome xenome.cc)

TARGET_LINK_LIBRARIES(xenome xenomeapp)


# translucent

ADD_LIBRARY(translucentapp
	TranslucentApp.cc GossOption.cc GossCmdReg.cc GossCmdHelp.cc 
	GossCmdBuildGraph.cc
	GossCmdLintGraph.cc
	GossCmdTrimGraph.cc
	GossCmdPruneTips.cc
	GossCmdPopBubbles.cc
	TransCmdAssemble.cc
	TransCmdMergeGraphWithReference.cc
	TransCmdTrimRelative.cc
	ResolveTranscripts.cc
)

if(BUILD_translucent)

TARGET_LINK_LIBRARIES(translucentapp gosslib ${Boost_PROGRAM_OPTIONS_LIBRARY})

ADD_EXECUTABLE(translucent translucent.cc)

TARGET_LINK_LIBRARIES(translucent translucentapp)

endif(BUILD_translucent)

# electus

ADD_LIBRARY(electusapp
	ElectApp.cc GossOption.cc GossCmdReg.cc GossCmdHelp.cc
	GossCmdBuildKmerSet.cc
	GossCmdFilterReads.cc
)

TARGET_LINK_LIBRARIES(electusapp gosslib ${Boost_PROGRAM_OPTIONS_LIBRARY})

ADD_EXECUTABLE(electus electus.cc)

TARGET_LINK_LIBRARIES(electus electusapp)


# espresso

# XXX Need to look for matio to build espresso
#if matio
#    ADD_LIBRARY(espressoapp
#        EspressoApp.cc GossOption.cc GossCmdReg.cc GossCmdHelp.cc
#        KmerSpectrum.cc
#    )
#    TARGET_LINK_LIBRARIES(espressoapp gosslib)
#    ADD_EXECUTABLE(espresso espressoapp)
#    TARGET_LINK_LIBRARIES(espresso matio ${Boost_PROGRAM_OPTIONS_LIBRARY})
#endif


# gossple

ADD_EXECUTABLE(gossple gossple.cc)

TARGET_LINK_LIBRARIES(gossple ${Boost_LIBRARIES})


# Install targets

install(TARGETS goss xenome electus gossple
	RUNTIME DESTINATION bin)


# Unit tests

if(BUILD_tests)

MACRO(gossamer_unit_test t src lib)
   ADD_EXECUTABLE(${t} ${src})
   TARGET_LINK_LIBRARIES(${t} ${lib})
   ADD_TEST(NAME ${t} COMMAND ${t})
ENDMACRO(gossamer_unit_test)

gossamer_unit_test(testAnotTree testAnnotTree.cc gosslib)
gossamer_unit_test(testAsyncMerge testAsyncMerge.cc gosslib)
gossamer_unit_test(testBackgroundLineSource testBackgroundLineSource.cc gosslib)
gossamer_unit_test(testBackyardHash testBackyardHash.cc gosslib)
gossamer_unit_test(testBigInteger testBigInteger.cc gosslib)
gossamer_unit_test(testBitVecSet testBitVecSet.cc gosslib)
gossamer_unit_test(testBlendedSort testBlendedSort.cc gosslib)
gossamer_unit_test(testBoundedQueue testBoundedQueue.cc gosslib)
gossamer_unit_test(testCompactDynamicBitVector testCompactDynamicBitVector.cc gosslib)
gossamer_unit_test(testDenseArray testDenseArray.cc gosslib)
gossamer_unit_test(testEdgeAndCount testEdgeAndCount.cc gosslib)
gossamer_unit_test(testEnumerativeCode testEnumerativeCode.cc gosslib)
gossamer_unit_test(testEstimateGraphStatistics testEstimateGraphStatistics.cc gosslib)
gossamer_unit_test(testExternalVarPushSorter testExternalVarPushSorter.cc gosslib)
gossamer_unit_test(testExternalBufferSort testExternalBufferSort.cc gosslib)
gossamer_unit_test(testFastqParser testFastqParser.cc gosslib)
gossamer_unit_test(testFeistelHash testFeistelHash.cc gosslib)
gossamer_unit_test(testFibHeap testFibHeap.cc gosslib)
gossamer_unit_test(testFixedWidthBitArray testFixedWidthBitArray.cc gosslib)
gossamer_unit_test(testGammaCodec testGammaCodec.cc gosslib)
gossamer_unit_test(testGossReadBaseString testGossReadBaseString.cc gosslib)
gossamer_unit_test(testGossReadSequenceBases testGossReadSequenceBases.cc gosslib)
gossamer_unit_test(testGraph testGraph.cc gosslib)
gossamer_unit_test(testJobManager testJobManager.cc gosslib)
gossamer_unit_test(testKmerAligner testKmerAligner.cc gossapp)
gossamer_unit_test(testKmerIndex testKmerIndex.cc gosslib)
gossamer_unit_test(testLevenbergMarquardt testLevenbergMarquardt.cc gosslib)
gossamer_unit_test(testLineParser testLineParser.cc gosslib)
gossamer_unit_test(testMultithreadedBatchTask testMultithreadedBatchTask.cc gosslib)
gossamer_unit_test(testPlainLineSource testPlainLineSource.cc gosslib)
gossamer_unit_test(testPhysicalFileFactory testPhysicalFileFactory.cc gosslib)
gossamer_unit_test(testRRRArray testRRRArray.cc gosslib)
gossamer_unit_test(testReverseComplementAdapter testReverseComplementAdapter.cc gosslib)
gossamer_unit_test(testRunLengthCodedBitVectorWord testRunLengthCodedBitVectorWord.cc gosslib)
gossamer_unit_test(testRunLengthCodedSet testRunLengthCodedSet.cc gosslib)
gossamer_unit_test(testSimpleHashMap testSimpleHashMap.cc gosslib)
gossamer_unit_test(testSimpleHashSet testSimpleHashSet.cc gosslib)
gossamer_unit_test(testSimpleRangeSet testSimpleRangeSet.cc gosslib)
gossamer_unit_test(testSmallBaseVector testSmallBaseVector.cc gosslib)
gossamer_unit_test(testSortedArrayMap testSortedArrayMap.cc gosslib)
gossamer_unit_test(testSparseArray testSparseArray.cc gosslib)
gossamer_unit_test(testSparseArrayView testSparseArrayView.cc gosslib)
gossamer_unit_test(testSpinlock testSpinlock.cc gosslib)
gossamer_unit_test(testTourBus testTourBus.cc gosslib)
gossamer_unit_test(testUtils testUtils.cc gosslib)
gossamer_unit_test(testVariableByteArray testVariableByteArray.cc gosslib)
gossamer_unit_test(testVariableWidthBitArray testVariableWidthBitArray.cc gosslib)
gossamer_unit_test(testWordyBitVector testWordyBitVector.cc gosslib)
gossamer_unit_test(testVByteCodec testVByteCodec.cc gosslib)
gossamer_unit_test(testGossCmdBuildGraph testGossCmdBuildGraph.cc gossapp)
gossamer_unit_test(testGossCmdPrintContigs testGossCmdPrintContigs.cc gossapp)

endif(BUILD_tests)

