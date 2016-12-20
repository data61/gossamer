
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
	${Boost_PROGRAM_OPTIONS_LIBRARY}
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

if(BUILD_translucent)

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

MACRO(gossamer_unit_test t src)
   ADD_EXECUTABLE(${t} ${src})
   ADD_TEST(NAME ${t} COMMAND ${t})

   set(extra_macro_args ${ARGN})
   list(LENGTH extra_macro_args num_extra_args)

   set(extra_libs gosslib)

   if (${num_extra_args} GREATER 0)
       list(GET extra_macro_args 0 lib)
       list(APPEND extra_libs ${lib})
   endif ()
   TARGET_LINK_LIBRARIES(${t} ${extra_libs})
ENDMACRO(gossamer_unit_test)

gossamer_unit_test(testAnotTree testAnnotTree.cc)
gossamer_unit_test(testAsyncMerge testAsyncMerge.cc)
gossamer_unit_test(testBackgroundLineSource testBackgroundLineSource.cc)
gossamer_unit_test(testBackyardHash testBackyardHash.cc)
gossamer_unit_test(testBigInteger testBigInteger.cc)
gossamer_unit_test(testBitVecSet testBitVecSet.cc)
gossamer_unit_test(testBlendedSort testBlendedSort.cc)
gossamer_unit_test(testBoundedQueue testBoundedQueue.cc)
gossamer_unit_test(testCompactDynamicBitVector testCompactDynamicBitVector.cc)
gossamer_unit_test(testDenseArray testDenseArray.cc)
gossamer_unit_test(testEdgeAndCount testEdgeAndCount.cc)
gossamer_unit_test(testEnumerativeCode testEnumerativeCode.cc)
gossamer_unit_test(testEstimateGraphStatistics testEstimateGraphStatistics.cc)
gossamer_unit_test(testExternalVarPushSorter testExternalVarPushSorter.cc)
gossamer_unit_test(testExternalBufferSort testExternalBufferSort.cc)
gossamer_unit_test(testFastqParser testFastqParser.cc)
gossamer_unit_test(testFeistelHash testFeistelHash.cc)
gossamer_unit_test(testFibHeap testFibHeap.cc)
gossamer_unit_test(testFixedWidthBitArray testFixedWidthBitArray.cc)
gossamer_unit_test(testGammaCodec testGammaCodec.cc)
gossamer_unit_test(testGossReadBaseString testGossReadBaseString.cc)
gossamer_unit_test(testGossReadSequenceBases testGossReadSequenceBases.cc)
gossamer_unit_test(testGraph testGraph.cc)
gossamer_unit_test(testJobManager testJobManager.cc)
gossamer_unit_test(testKmerAligner testKmerAligner.cc gossapp)
gossamer_unit_test(testKmerIndex testKmerIndex.cc)
gossamer_unit_test(testLevenbergMarquardt testLevenbergMarquardt.cc)
gossamer_unit_test(testLineParser testLineParser.cc)
gossamer_unit_test(testMultithreadedBatchTask testMultithreadedBatchTask.cc)
gossamer_unit_test(testPlainLineSource testPlainLineSource.cc)
gossamer_unit_test(testPhysicalFileFactory testPhysicalFileFactory.cc)
gossamer_unit_test(testRRRArray testRRRArray.cc)
gossamer_unit_test(testReverseComplementAdapter testReverseComplementAdapter.cc)
gossamer_unit_test(testRunLengthCodedBitVectorWord testRunLengthCodedBitVectorWord.cc)
gossamer_unit_test(testRunLengthCodedSet testRunLengthCodedSet.cc)
gossamer_unit_test(testSimpleHashMap testSimpleHashMap.cc)
gossamer_unit_test(testSimpleHashSet testSimpleHashSet.cc)
gossamer_unit_test(testSimpleRangeSet testSimpleRangeSet.cc)
gossamer_unit_test(testSmallBaseVector testSmallBaseVector.cc)
gossamer_unit_test(testSortedArrayMap testSortedArrayMap.cc)
gossamer_unit_test(testSparseArray testSparseArray.cc)
gossamer_unit_test(testSparseArrayView testSparseArrayView.cc)
gossamer_unit_test(testSpinlock testSpinlock.cc)
gossamer_unit_test(testTourBus testTourBus.cc)
gossamer_unit_test(testUtils testUtils.cc)
gossamer_unit_test(testVariableByteArray testVariableByteArray.cc)
gossamer_unit_test(testVariableWidthBitArray testVariableWidthBitArray.cc)
gossamer_unit_test(testWordyBitVector testWordyBitVector.cc)
gossamer_unit_test(testVByteCodec testVByteCodec.cc)
gossamer_unit_test(testGossCmdBuildGraph testGossCmdBuildGraph.cc gossapp)
gossamer_unit_test(testGossCmdPrintContigs testGossCmdPrintContigs.cc gossapp)

endif(BUILD_tests)
