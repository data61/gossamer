// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
// TODO: 
//  - Fix for different Ks!
//  - Selectable filter predicate, e.g. keep reads that match <= N references, rather than >= N.
//    (this probably isn't so useful since we'll generally also want at least 1 reference hit.)
//  - Alternative: user-specified reference count range, e.g. the interval [min, max].
//    Currently the default is [R, R], where R is the number of references specified.
//    Specifying --reference-threshold T1 gives us [T1, R].
//    We can add another flag, e.g. --reference-threshold-upper T2, which alone would give [1, T2],
//    and in combination with --reference-threshold T1 would yield [T1, T2].
//    (Add --reference-threshold-lower and make --reference-threshold a synonym or remove it.)
//  - Eliminate the 64 reference limit - we only care about the numbers of references.

#include "ElectApp.hh"

#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "BackyardHash.hh"
#include "Debug.hh"
#include "ElectVersion.hh"
#include "FastaParser.hh"
#include "FileFactory.hh"
#include "GossamerException.hh"
#include "GossCmdBuildKmerSet.hh"
#include "GossCmdReg.hh"
#include "GossOption.hh"
#include "GossOptionChecker.hh"
#include "GossRead.hh"
#include "GossReadHandler.hh"
#include "GossReadProcessor.hh"
#include "GossReadSequenceBases.hh"
#include "KmerSet.hh"
#include "KmerizingAdapter.hh"
#include "Logger.hh"
#include "PhysicalFileFactory.hh"
#include "Timer.hh"
#include "ReadSequenceFileSequence.hh"
#include "SimpleHashMap.hh"
#include "StringFileFactory.hh"
#include "UnboundedProgressMonitor.hh"
#include "Utils.hh"

#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>

#include <thread>

#include "GossCmdBuildKmerSet.hh"
#include "GossCmdFilterReads.hh"
#include "GossCmdHelp.hh"

using namespace boost;
using namespace boost::program_options;
using namespace std;
using namespace Gossamer;

typedef vector<string> strings;

typedef std::pair<GossReadPtr, GossReadPtr> ReadPair;

namespace // anonymous
{
    GossOptions globalOpts;

    GossOptions commonOpts;

    vector<GossCmdReg> cmds;

    class ElectCmdIndex : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            Logger& log(pCxt.log);

            // Assume 0.2 GB operating overhead.
            const uint64_t M = (mM - 0.2) * 1024 * 1024 * 1024;
            const uint64_t S = BackyardHash::maxSlotBits(M);
            const uint64_t N = M / (1.5 * sizeof(uint32_t) + sizeof(BackyardHash::value_type));

            log(info, "building reference kmer set");
            GossCmdBuildKmerSet(mK, S, N, mT, mOut, mRefFastas, strings(), strings())(pCxt);

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        ElectCmdIndex(const strings& pRefFasta, const string& pOut, uint64_t pK, double pM, uint64_t pT)
            : mRefFastas(pRefFasta), mOut(pOut), mK(pK), mM(pM), mT(pT)
        {
        }

    private:
        const strings mRefFastas;
        const string mOut;
        const uint64_t mK;
        const double mM;
        const uint64_t mT;
    };

    class ElectCmdFactoryIndex : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            uint64_t K = 25;
            chk.getOptional("kmer-size", K);

            strings refs;
            chk.getRepeating1("ref-fasta", refs, readChk);

            string out;
            chk.getMandatory("prefix", out);

            double M = 2;
            chk.getOptional("max-memory", M);

            uint64_t T = 4;
            chk.getOptional("num-threads", T);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new ElectCmdIndex(refs, out, K, M, T));
        }

        ElectCmdFactoryIndex()
            : GossCmdFactory("build an index for classifying reads")
        {
            mCommonOptions.insert("ref-fasta");
            mCommonOptions.insert("max-memory");
            mCommonOptions.insert("kmer-size");
            mSpecificOptions.addOpt<string>("prefix", "P", "reference output prefix");
        }
    };

    // Group reads wrt a set of references.
    class ElectCmdGroup : public GossCmd
    {
        typedef std::vector<std::string> strings;
        typedef std::shared_ptr<KmerSet> KmerSetPtr;
        // typedef SimpleHashMap<Gossamer::edge_type, uint64_t> KmerMap;

        // Maps kmers to bitsets representing the references they appear in.
        struct KmerMap
        {
            uint64_t operator[](const Gossamer::edge_type& pKmer) const
            {
                Gossamer::edge_type kmer(pKmer.normalized(mK));
                uint64_t c = 0;
                uint64_t m = 1;
                for (uint8_t i = 0; i < mKmerSets.size(); ++i)
                {
                    if (mKmerSets[i]->access(KmerSet::Edge(kmer)))
                    {
                        c |= m;
                    }
                    m <<= 1;
                }
                return c;
            }

            // Add a single read as a reference.
            void addReference(GossCmdContext& pCxt, uint8_t pId, const GossRead& pRead, const uint64_t pNumThreads)
            {
                // const uint64_t B = 1;
                const uint64_t bytes = 10 << 20;
                const uint64_t S = BackyardHash::maxSlotBits(bytes);
                const uint64_t N = (bytes) / (1.5 * sizeof(uint32_t) + sizeof(BackyardHash::value_type));
                string name(lexical_cast<string>(pId));
                GossCmdBuildKmerSet cmd(mK, S, N, pNumThreads, name);
                GossRead::Iterator src(pRead, mK);
                cmd(pCxt, src);
                
                KmerSetPtr kmerSetPtr = std::make_shared<KmerSet>(name, pCxt.fac);
                if (mKmerSets.size() < (uint64_t(pId) + 1))
                {
                    mKmerSets.resize(pId + 1);
                }
                mKmerSets[pId] = kmerSetPtr;
            }

            // Add a FASTA file as a reference.
            void addReference(GossCmdContext& pCxt, FileFactory& pSrcFac, uint8_t pId, const string& pFastaFile, const uint64_t pNumThreads)
            {
                // const uint64_t B = 12;
                const uint64_t bytes = 10 << 20;
                const uint64_t S = BackyardHash::maxSlotBits(bytes);
                const uint64_t N = (bytes) / (1.5 * sizeof(uint32_t) + sizeof(BackyardHash::value_type));
                string name(lexical_cast<string>(pId));

                std::deque<GossReadSequence::Item> items;

                auto seqFac = std::make_shared<GossReadSequenceBasesFactory>();
                GossReadParserFactory fastaParserFac(FastaParser::create);

                items.push_back(GossReadSequence::Item(pFastaFile, fastaParserFac, seqFac));
                LineSourceFactory lineSrcFac(BackgroundLineSource::create);
                ReadSequenceFileSequence reads(items, pSrcFac, lineSrcFac);
                KmerizingAdapter src(reads, mK);

                GossCmdBuildKmerSet cmd(mK, S, N, pNumThreads, name);
                cmd(pCxt, src);
                
                KmerSetPtr kmerSetPtr(new KmerSet(name, pCxt.fac));
                if (mKmerSets.size() < (uint64_t(pId) + 1))
                {
                    Gossamer::ensureCapacity(mKmerSets);
                    mKmerSets.resize(pId + 1);
                }
                mKmerSets[pId] = kmerSetPtr;
            }

            // Add a pre-built KmerSet index as a reference.
            void addReference(FileFactory& pFac, const string& pBaseName, uint8_t pId)
            {
                KmerSetPtr kmerSetPtr(new KmerSet(pBaseName, pFac));
                if (mKmerSets.size() < (uint64_t(pId) + 1))
                {
                    Gossamer::ensureCapacity(mKmerSets);
                    mKmerSets.resize(pId + 1);
                }
                mKmerSets[pId] = kmerSetPtr;
            }

            KmerMap(const uint64_t pK)
                : mK(pK), mKmerSets()
            {
                mKmerSets.reserve(64);
            }

        private:

            // TODO: This ought to be a multimap from k to the sets at that k!
            const uint64_t mK;
            vector<KmerSetPtr> mKmerSets;
        };

        struct RefCompiler : public GossReadHandler
        {
            // Add a KmerSet index reference.
            void operator()(const string& pPrefix, FileFactory& pFac)
            {
                mKmerMap.addReference(pFac, pPrefix, mId);
                ++mId;
            }

            // Add a FASTA file reference.
            void operator()(FileFactory& pSrcFac, const string& pFastaFile)
            {
                mKmerMap.addReference(mCxt, pSrcFac, mId, pFastaFile, mNumThreads);
                ++mId;
            }

            // Add a single read reference.
            void operator()(const GossRead& pRead)
            {
                mKmerMap.addReference(mCxt, mId, pRead, mNumThreads);
                ++mId;
            }

            void operator()(const GossRead& pLhs, const GossRead& pRhs)
            {
                BOOST_ASSERT(false);
            }

            void end()
            {
            }

            RefCompiler(GossCmdContext& pCxt, const uint64_t pK, KmerMap& pKmerMap, uint64_t pNumThreads)
                : mId(0), mK(pK), mNumThreads(pNumThreads), mKmerMap(pKmerMap), mCxt(pCxt)
            {
            }

        private:
            uint8_t mId;
            const uint64_t mK;
            const uint64_t mNumThreads;
            KmerMap& mKmerMap;
            GossCmdContext& mCxt;
        };

        struct ClassifiedReadWriter
        {
            // Write all buffered matched/nonmatched reads.
            void flush(bool pMatched)
            {
                stringstream& buf1(pMatched ? mMatchBuffer1 : mNonmatchBuffer1);
                stringstream& buf2(pMatched ? mMatchBuffer2 : mNonmatchBuffer2);
                ostream* out1(pMatched ? mMatchOut1 : mNonmatchOut1);
                ostream* out2(pMatched ? mMatchOut2 : mNonmatchOut2);
                uint64_t& num(pMatched ? mMatchBuffered : mNonmatchBuffered);
                mutex& mut(pMatched ? mMatchMut : mNonmatchMut);

                num = 0;
                {
                    unique_lock<mutex> lk(mut);
                    if (out1)
                    {
                        *out1 << buf1.rdbuf();
                    }
                    if (out2)
                    {
                        *out2 << buf2.rdbuf();
                    }
                }
                buf1.str("");
                buf2.str("");
            }

            // Write all buffered reads.
            void flush()
            {
                flush(true);
                flush(false);
            }

            void operator()(bool pMatched, const GossRead& pRead)
            {
                // Don't bother if there's no output stream behind the buffer.
                if (   ( pMatched && !mMatchOut1)
                    || (!pMatched && !mNonmatchOut1))
                {
                    return;
                }

                uint64_t& num(pMatched ? mMatchBuffered : mNonmatchBuffered);
                if (num >= mBufferSize)
                {
                    flush(pMatched);
                }

                num += 1;
                stringstream& buf(pMatched ? mMatchBuffer1 : mNonmatchBuffer1);
                pRead.print(buf);
            }

            void operator()(bool pMatched, const GossRead& pRead1, const GossRead& pRead2)
            {
                // Don't bother if there's no output stream behind the buffer.
                if (   ( pMatched && !mMatchOut1)
                    || (!pMatched && !mNonmatchOut1))
                {
                    return;
                }

                uint64_t& num(pMatched ? mMatchBuffered : mNonmatchBuffered);
                if (num >= mBufferSize)
                {
                    flush(pMatched);
                }

                num += 1;
                stringstream& buf1(pMatched ? mMatchBuffer1 : mNonmatchBuffer1);
                stringstream& buf2(pMatched ? mMatchBuffer2 : mNonmatchBuffer2);
                pRead1.print(buf1);
                pRead2.print(buf2);
            }

            ~ClassifiedReadWriter()
            {
                flush();
            }

            ClassifiedReadWriter(mutex& pMatchMut, mutex& pNonmatchMut,
                                 ostream* pMatchOut1, ostream* pMatchOut2, 
                                 ostream* pNonmatchOut1, ostream* pNonmatchOut2,
                                 uint64_t pBufferSize)
                : mMatchBuffered(0), mNonmatchBuffered(0), mBufferSize(pBufferSize),
                  mMatchMut(pMatchMut), mNonmatchMut(pNonmatchMut),
                  mMatchOut1(pMatchOut1), mMatchOut2(pMatchOut2),
                  mNonmatchOut1(pNonmatchOut1), mNonmatchOut2(pNonmatchOut2),
                  mMatchBuffer1(), mMatchBuffer2(), mNonmatchBuffer1(), mNonmatchBuffer2()
            {
            }

        private:
            uint64_t mMatchBuffered;
            uint64_t mNonmatchBuffered;
            const uint64_t mBufferSize;
            mutex& mMatchMut;
            mutex& mNonmatchMut;
            ostream* mMatchOut1;
            ostream* mMatchOut2;
            ostream* mNonmatchOut1;
            ostream* mNonmatchOut2;
            stringstream mMatchBuffer1;
            stringstream mMatchBuffer2;
            stringstream mNonmatchBuffer1;
            stringstream mNonmatchBuffer2;
        };

        struct KmerFilter
        {
            void push_back(GossReadPtr pRead)
            {
                const GossRead& read(*pRead);
                uint64_t c = 0;
                for (GossRead::Iterator i(read, mK); i.valid(); ++i)
                {
                    c |= mKmerMap[i.kmer().normalized(mK)];
                    if (popcnt(c) >= mRefThreshold)
                    {
                        mWriter(true, read);
                        return;
                    }
                }

                mWriter(false, read);
            }

            void push_back(ReadPair pPair)
            {
                const GossRead& lhs(*pPair.first);
                const GossRead& rhs(*pPair.second);
                uint64_t c = 0;
                for (GossRead::Iterator i(lhs, mK); i.valid(); ++i)
                {
                    c |= mKmerMap[i.kmer().normalized(mK)];
                    if (popcnt(c) >= mRefThreshold)
                    {
                        mWriter(true, lhs, rhs);
                        return;
                    }
                }
                
                for (GossRead::Iterator i(rhs, mK); i.valid(); ++i)
                {
                    c |= mKmerMap[i.kmer().normalized(mK)];
                    if (c >= mRefThreshold)
                    {
                        mWriter(true, lhs, rhs);
                        return;
                    }
                }

                mWriter(false, lhs, rhs);
            }

            KmerFilter(const uint64_t pK, const uint64_t pRefThreshold,
                       const KmerMap& pKmerMap, ostream* pMatchOutLhs, ostream* pMatchOutRhs, 
                       ostream* pNonmatchOutLhs, ostream* pNonmatchOutRhs,
                       mutex& pMatchMut, mutex& pNonmatchMut)
                : mK(pK), mRefThreshold(pRefThreshold), mKmerMap(pKmerMap),
                  mWriter(pMatchMut, pNonmatchMut, pMatchOutLhs, pMatchOutRhs, pNonmatchOutLhs, pNonmatchOutRhs, 256)
            {
            }

        private:

            // TODO: Introduce a multimap from k to KmerMaps at that k!
            const uint64_t mK;
            const uint64_t mRefThreshold;
            const KmerMap& mKmerMap;
            ClassifiedReadWriter mWriter;
        };

        typedef std::shared_ptr<KmerFilter> KmerFilterPtr;

        struct ReadFilter : public GossReadHandler
        {
            void operator()(const GossRead& pRead)
            {
                mReadGrp.push_back(pRead.clone());
            }

            void operator()(const GossRead& pLhs, const GossRead& pRhs)
            {
                mPairGrp.push_back(ReadPair(pLhs.clone(), pRhs.clone()));
            }

            ReadFilter(const uint64_t pK, const uint64_t pRefThreshold, 
                       const KmerMap& pKmerMap, ostream* pMatchOut, ostream* pNonmatchOut,
                       uint64_t pNumThreads)
                : mK(pK), mRefThreshold(pRefThreshold), 
                  mKmerMap(pKmerMap), mMatchOut1(pMatchOut), mMatchOut2(0), 
                  mNonmatchOut1(pNonmatchOut), mNonmatchOut2(0),
                  mReadGrp(128), mPairGrp(0), mNumThreads(pNumThreads)
            {
                for (uint64_t i = 0; i < mNumThreads; ++i)
                {
                    mKmerFilts.push_back(
                            std::make_shared<KmerFilter>(
                                mK, mRefThreshold, mKmerMap,
                                mMatchOut1, mMatchOut2, mNonmatchOut1, mNonmatchOut2,
                                mMatchMut, mNonmatchMut));
                    mReadGrp.add(*mKmerFilts.back());
                }
            }

            ReadFilter(const uint64_t pK, const uint64_t pRefThreshold,
                       const KmerMap& pKmerMap, ostream* pMatchOutLhs, ostream* pMatchOutRhs, 
                       ostream* pNonmatchOutLhs, ostream* pNonmatchOutRhs, uint64_t pNumThreads)
                : mK(pK), mRefThreshold(pRefThreshold),
                  mKmerMap(pKmerMap), mMatchOut1(pMatchOutLhs), mMatchOut2(pMatchOutRhs), 
                  mNonmatchOut1(pNonmatchOutLhs), mNonmatchOut2(pNonmatchOutRhs),
                  mReadGrp(0), mPairGrp(128), mNumThreads(pNumThreads)
            {
                for (uint64_t i = 0; i < mNumThreads; ++i)
                {
                    mKmerFilts.push_back(
                            std::make_shared<KmerFilter>(
                                mK, mRefThreshold, mKmerMap,
                                mMatchOut1, mMatchOut2, mNonmatchOut1, mNonmatchOut2,
                                mMatchMut, mNonmatchMut));
                    mPairGrp.add(*mKmerFilts.back());
                }
            }

        private:
            const uint64_t mK;
            const uint64_t mRefThreshold;
            const KmerMap& mKmerMap;
            mutex mMatchMut;
            mutex mNonmatchMut;
            ostream* mMatchOut1;
            ostream* mMatchOut2;
            ostream* mNonmatchOut1;
            ostream* mNonmatchOut2;
            vector<KmerFilterPtr> mKmerFilts;
            BackgroundMultiConsumer<GossReadPtr> mReadGrp;
            BackgroundMultiConsumer<ReadPair> mPairGrp;
            const uint64_t mNumThreads;
        };

        void filterSingle(const GossCmdContext& pCxt, const KmerMap& pKmerMap, const string& pSuffix, 
                          const strings& pFastas, const strings& pFastqs, const strings& pLines,
                          uint64_t pNumThreads)
        {
            if (pFastas.empty() && pFastqs.empty() && pLines.empty())
            {
                return;
            }
            Logger& log(pCxt.log);
            FileFactory& fac(pCxt.fac);
            UnboundedProgressMonitor umon(log, 100000, " reads");
            FileFactory::OutHolderPtr matchPtr;
            FileFactory::OutHolderPtr nonmatchPtr;
            if (mMatchPrefix.size())
            {
                matchPtr = fac.out(mMatchPrefix + pSuffix);
            }
            if (mNonmatchPrefix.size())
            {
                nonmatchPtr = fac.out(mNonmatchPrefix + pSuffix);
            }
            ReadFilter filt(mK, mRefThreshold, pKmerMap, 
                            matchPtr ? &**matchPtr : 0, nonmatchPtr ? &**nonmatchPtr : 0,
                            pNumThreads);
            GossReadProcessor::processSingle(pCxt, pFastas, pFastqs, pLines, filt, &umon);
        }

        void filterPairs(const GossCmdContext& pCxt, const KmerMap& pKmerMap, const string& pSuffix, 
                         const strings& pFastas, const strings& pFastqs, const strings& pLines,
                         uint64_t pNumThreads)
        {
            if (pFastas.empty() && pFastqs.empty() && pLines.empty())
            {
                return;
            }
            Logger& log(pCxt.log);
            FileFactory& fac(pCxt.fac);
            UnboundedProgressMonitor umon(log, 100000, " read pairs");
            FileFactory::OutHolderPtr matchLhsPtr;
            FileFactory::OutHolderPtr matchRhsPtr;
            FileFactory::OutHolderPtr nonmatchLhsPtr;
            FileFactory::OutHolderPtr nonmatchRhsPtr;
            if (mMatchPrefix.size())
            {
                matchLhsPtr = fac.out(mMatchPrefix + "_1" + pSuffix);
                matchRhsPtr = fac.out(mMatchPrefix + "_2" + pSuffix);
            }
            if (mNonmatchPrefix.size())
            {
                nonmatchLhsPtr = fac.out(mNonmatchPrefix + "_1" + pSuffix);
                nonmatchRhsPtr = fac.out(mNonmatchPrefix + "_2" + pSuffix);
            }
            ReadFilter filt(mK, mRefThreshold, pKmerMap, 
                            matchLhsPtr ? &**matchLhsPtr : 0, matchRhsPtr ? &**matchRhsPtr : 0, 
                            nonmatchLhsPtr ? &**nonmatchLhsPtr : 0, nonmatchRhsPtr ? &**nonmatchRhsPtr : 0,
                            pNumThreads);
            GossReadProcessor::processPairs(pCxt, pFastas, pFastqs, pLines, filt, &umon);
        }

    public:

        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            Logger& log(pCxt.log);
            FileFactory& fac(pCxt.fac);
            string kmerSetName(fac.tmpName());

            auto strFacPtr = std::make_shared<StringFileFactory>();
            GossCmdContext refCxt(*strFacPtr, pCxt.log, pCxt.cmdName, pCxt.opts);

            vector<string> refNames;
            KmerMap kmerMap(mK);
            RefCompiler refCompiler(refCxt, mK, kmerMap, mNumThreads);
    
            refNames.reserve(mRefFastas.size());
            for (uint64_t i = 0; i < mRefFastas.size(); ++i)
            {
                const string& fasta(mRefFastas[i]);
                size_t lastDot = fasta.find_last_of(fasta, '.');
                string name = lastDot == string::npos ? fasta : fasta.substr(0, lastDot);
                refNames.push_back(name);
            }

            // Read each reference.
            log(info, "reading references");
            if (!mRefFastas.empty())
            {
                UnboundedProgressMonitor umon(log, 1, " FASTA reference(s)");
                if (mSingleSeqRefs)
                {
                    // Each sequence is a reference.
                    GossReadProcessor::processSingle(pCxt, mRefFastas, strings(), strings(), refCompiler, &umon);
                }
                else
                {
                    // Each file is a reference.
                    for (uint64_t i = 0; i < mRefFastas.size(); ++i)
                    {   
                        umon.tick(i + 1);
                        refCompiler(fac, mRefFastas[i]);
                    }
                }
            }
            if (!mRefIndexes.empty())
            {
                UnboundedProgressMonitor umon(log, 1, " index reference(s)");
                for (uint64_t i = 0; i < mRefIndexes.size(); ++i)
                {
                    umon.tick(i + 1);
                    refCompiler(mRefIndexes[i], fac);
                }
            }
            
            log(info, "processing reads");
            // Process reads.
            if (mPairs)
            {
                filterPairs(pCxt, kmerMap, ".fasta", mFastas, strings(), strings(), mNumThreads);
                filterPairs(pCxt, kmerMap, ".fastq", strings(), mFastqs, strings(), mNumThreads);
                filterPairs(pCxt, kmerMap, ".txt", strings(), strings(), mLines, mNumThreads);
            }
            else
            {
                filterSingle(pCxt, kmerMap, ".fasta", mFastas, strings(), strings(), mNumThreads);
                filterSingle(pCxt, kmerMap, ".fastq", strings(), mFastqs, strings(), mNumThreads);
                filterSingle(pCxt, kmerMap, ".txt", strings(), strings(), mLines, mNumThreads);
            }
            
            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        ElectCmdGroup(const uint64_t& pK, const uint64_t& pRefThreshold, 
                      const strings& pRefFastas, const strings& pRefIndexes,
                      const strings& pFastas, const strings& pFastqs, const strings& pLines,
                      bool pPairs, double pMaxMemory, uint64_t pNumThreads,
                      const string& pMatchPrefix, const string& pNonmatchPrefix,
                      bool pPreserveReadOrder, bool pSingleSeqRefs)
            : mK(pK), mRefThreshold(pRefThreshold == -1ULL ? pRefFastas.size() + pRefIndexes.size() : pRefThreshold),
              mRefFastas(pRefFastas), mRefIndexes(pRefIndexes),
              mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
              mPairs(pPairs), mMaxMemory(pMaxMemory), mNumThreads(pNumThreads),
              mMatchPrefix(pMatchPrefix), mNonmatchPrefix(pNonmatchPrefix),
              mPreserveReadOrder(pPreserveReadOrder), mSingleSeqRefs(pSingleSeqRefs)
        {
        }

    private:
        const uint64_t mK;
        const uint64_t mRefThreshold;
        const strings mRefFastas;
        const strings mRefIndexes;
        const strings mFastas;
        const strings mFastqs;
        const strings mLines;
        const bool mPairs;
        const uint64_t mMaxMemory;
        const uint64_t mNumThreads;
        const string mMatchPrefix;
        const string mNonmatchPrefix;
        const bool mPreserveReadOrder;
        const bool mSingleSeqRefs;
    };

    class ElectCmdFactoryGroup : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            strings fastaRefs;
            chk.getRepeating0("ref-fasta", fastaRefs, readChk);

            strings indexRefs;
            chk.getRepeating0("ref-index", indexRefs);

            strings fastas;
            chk.getRepeating0("fasta-in", fastas, readChk);

            strings fastqs;
            chk.getRepeating0("fastq-in", fastqs, readChk);

            strings lines;
            chk.getRepeating0("line-in", lines, readChk);

            uint64_t K = 25;
            chk.getOptional("kmer-size", K);

            bool pairs;
            chk.getOptional("pairs", pairs);

            double maxMem = 1.0e9;
            chk.getOptional("max-memory", maxMem);

            uint64_t T = 4;
            chk.getOptional("num-threads", T);

            uint64_t refThresh = -1ULL;
            chk.getOptional("ref-threshold", refThresh);

            string match = "";
            chk.getOptional("match-prefix", match);

            string nonmatch = "";
            chk.getOptional("non-match-prefix", nonmatch);

            bool ord = false;
            chk.getOptional("preserve-read-order", ord);

            bool singleSeqRefs = false;
            chk.getOptional("single-sequence-refs", singleSeqRefs);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new ElectCmdGroup(K, refThresh, fastaRefs, indexRefs, fastas, fastqs, lines, pairs, maxMem, T, 
                                                match, nonmatch, ord, singleSeqRefs));
        }

        ElectCmdFactoryGroup()
            : GossCmdFactory("classify reads according to reference")
        {
            mCommonOptions.insert("ref-fasta");
            mCommonOptions.insert("fasta-in");
            mCommonOptions.insert("fastq-in");
            mCommonOptions.insert("line-in");
            mCommonOptions.insert("max-memory");
            mCommonOptions.insert("kmer-size");
            
            mSpecificOptions.addOpt<bool>("pairs", "",
                    "treat reads as pairs");
            mSpecificOptions.addOpt<bool>("dont-write-reads", "",
                    "do not produce any output read files");
            mSpecificOptions.addOpt<bool>("preserve-read-order", "",
                    "maintain the same relative ordering of reads in output files");
            mSpecificOptions.addOpt<uint64_t>("ref-threshold", "",
                    "the minimum number of references a read must match (default: all of them)");
            mSpecificOptions.addOpt<string>("match-prefix", "", "filename prefix for matching reads");
            mSpecificOptions.addOpt<string>("non-match-prefix", "", "filename prefix for non-matching reads");
            mSpecificOptions.addOpt<string>("single-seq-refs", "", "treat each sequence as a separate reference");
            mSpecificOptions.addOpt<strings>("ref-index", "", "prefix of reference index");
        }
    };

} // namespace anonymous

const char*
ElectApp::name() const
{
    return "electus";
}

const char*
ElectApp::version() const
{
    return Gossamer::version;
}

ElectApp::ElectApp()
    : App(globalOpts, commonOpts)
{
    cmds.push_back(GossCmdReg("classify", GossCmdFactoryPtr(new ElectCmdFactoryGroup)));
    cmds.push_back(GossCmdReg("index", GossCmdFactoryPtr(new ElectCmdFactoryIndex)));
    cmds.push_back(GossCmdReg("help", GossCmdFactoryPtr(new GossCmdFactoryHelp(*this))));

    globalOpts.addOpt<strings>("debug", "D",
                                "enable particular debugging output");
    globalOpts.addOpt<bool>("help", "h", "show a help message");
    globalOpts.addOpt<string>("log-file", "l", "place to write messages");
    globalOpts.addOpt<strings>("tmp-dir", "", "a directory to use for temporary files (default /tmp)");
    globalOpts.addOpt<uint64_t>("num-threads", "T", "maximum number of worker threads to use, where possible");
    globalOpts.addOpt<bool>("verbose", "v", "show progress messages");
    globalOpts.addOpt<bool>("version", "V", "show the software version");

    commonOpts.addOpt<strings>("ref-fasta", "", "reference file in FASTA format");
    commonOpts.addOpt<strings>("fasta-in", "I", "input file in FASTA format");
    commonOpts.addOpt<strings>("fastq-in", "i", "input file in FASTQ format");
    commonOpts.addOpt<strings>("line-in", "", "input file with one sequence per line");
    commonOpts.addOpt<double>("max-memory", "M", "maximum memory (in GB) to use");
    commonOpts.addOpt<uint64_t>("kmer-size", "K", "kmer size to use (default 25)");
}
