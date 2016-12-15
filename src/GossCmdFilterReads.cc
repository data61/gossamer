// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdFilterReads.hh"

#include "BackgroundMultiConsumer.hh"
#include "LineSource.hh"
#include "Debug.hh"
#include "KmerSet.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "LineParser.hh"
#include "ProgressMonitor.hh"
#include "ReadPairSequenceFileSequence.hh"
#include "ReadSequenceFileSequence.hh"
#include "Spinlock.hh"
#include "SimpleHashSet.hh"
#include "Timer.hh"

#include <iostream>
#include <map>

using namespace boost;
using namespace std;

typedef vector<string> strings;
typedef pair<uint64_t,Gossamer::position_type> KmerItem;
typedef vector<KmerItem> KmerItems;

typedef map<uint64_t, uint64_t> Hist;
typedef map<uint64_t, map<uint64_t, uint64_t> > Hist2;

namespace // anonymous
{
    class ReadAligner
    {
    public:
        void push_back(const GossReadPtr& pRead)
        {
            const uint64_t rho = mKmerSet.K() + 1;

            for(GossRead::Iterator itr(*pRead, rho); itr.valid(); ++itr)
            {
                KmerSet::Edge e(itr.kmer());
                uint64_t rnk;
                if (mKmerSet.accessAndRank(e, rnk))
                {
                    if (mMatchOut)
                    {
                        std::unique_lock<std::mutex> lock(mMutex);
                        pRead->print(*mMatchOut);
                    }
                    return;
                }
                KmerSet::Edge e_rc = mKmerSet.reverseComplement(e);
                if (mKmerSet.accessAndRank(e_rc, rnk))
                {
                    if (mMatchOut)
                    {
                        std::unique_lock<std::mutex> lock(mMutex);
                        pRead->print(*mMatchOut);
                    }
                    return;
                }
            }
            if (mNonMatchOut)
            {
                std::unique_lock<std::mutex> lock(mMutex);
                pRead->print(*mNonMatchOut);
            }
        }

        ReadAligner(const KmerSet& pKmerSet, mutex& pMutex, ostream* pMatchOut, ostream* pNonMatchOut)
            : mKmerSet(pKmerSet), mMutex(pMutex), mMatchOut(pMatchOut), mNonMatchOut(pNonMatchOut)
        {
        }

    private:
        const KmerSet& mKmerSet;
        mutex& mMutex;
        ostream* mMatchOut;
        ostream* mNonMatchOut;
    };
    typedef std::shared_ptr<ReadAligner> ReadAlignerPtr;

    class PairAligner
    {
    public:
        void push_back(const pair<GossReadPtr, GossReadPtr>& pPair)
        {
            if (match(*(pPair.first)) || match(*(pPair.second)))
            {
                std::unique_lock<std::mutex> lock(mMutex);
                if (mMatchOut1)
                {
                    pPair.first->print(*mMatchOut1);
                }
                if (mMatchOut2)
                {
                    pPair.second->print(*mMatchOut2);
                }
            }
            else 
            {
                std::unique_lock<std::mutex> lock(mMutex);
                if (mNonMatchOut1)
                {
                    pPair.first->print(*mNonMatchOut1);
                }
                if (mNonMatchOut2)
                {
                    pPair.second->print(*mNonMatchOut2);
                }
            }
        }

        PairAligner(const KmerSet& pKmerSet, mutex& pMutex, 
                    ostream* pMatchOut1, ostream* pMatchOut2,
                    ostream* pNonMatchOut1, ostream* pNonMatchOut2)
            : mKmerSet(pKmerSet), mMutex(pMutex),
              mMatchOut1(pMatchOut1), mMatchOut2(pMatchOut2),
              mNonMatchOut1(pNonMatchOut1), mNonMatchOut2(pNonMatchOut2)
        {
        }

    private:

        bool match(const GossRead& pRead) const
        {
            const uint64_t k = mKmerSet.K();

            for(GossRead::Iterator itr(pRead, k); itr.valid(); ++itr)
            {
                Gossamer::position_type kmer(itr.kmer());
                kmer.normalize(k);
                KmerSet::Edge e(itr.kmer());
                uint64_t rnk;
                if (mKmerSet.accessAndRank(e, rnk))
                {
                    return true;
                }
            }
            
            return false;
        }

        const KmerSet& mKmerSet;
        mutex& mMutex;
        ostream* mMatchOut1;
        ostream* mMatchOut2;
        ostream* mNonMatchOut1;
        ostream* mNonMatchOut2;
    };
    typedef std::shared_ptr<PairAligner> PairAlignerPtr;

void
pairFiles(const string& pBaseName, string& pName1, string& pName2)
{
    size_t lastDot = pBaseName.find_last_of(".");
    string pre = pBaseName.substr(0, lastDot);
    string suf = pBaseName.substr(lastDot);
    pName1 = pre + "_1" + suf;
    pName2 = pre + "_2" + suf;
}

} // namespace anonymous

void
GossCmdFilterReads::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);;

    Timer t;

    KmerSet g(mIn, fac);

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

    mutex m;

    if (mPairs)
    {
        UnboundedProgressMonitor umon(log, 100000, " read pairs");
        LineSourceFactory lineSrcFac(BackgroundLineSource::create);
        ReadPairSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);

        FileFactory::OutHolderPtr match1P;
        FileFactory::OutHolderPtr match2P;
        FileFactory::OutHolderPtr nonMatch1P;
        FileFactory::OutHolderPtr nonMatch2P;
        ostream* match1Str = NULL;
        ostream* match2Str = NULL;
        ostream* nonMatch1Str = NULL;
        ostream* nonMatch2Str = NULL;
        if (mMatch.size())
        {
            string match1;
            string match2;
            pairFiles(mMatch, match1, match2);
            match1P = fac.out(match1);
            match2P = fac.out(match2);
            match1Str = &**match1P;
            match2Str = &**match2P;
        }
        if (mNonMatch.size())
        {
            string nonMatch1;
            string nonMatch2;
            pairFiles(mNonMatch, nonMatch1, nonMatch2);
            nonMatch1P = fac.out(nonMatch1);
            nonMatch2P = fac.out(nonMatch2);
            nonMatch1Str = &**nonMatch1P;
            nonMatch2Str = &**nonMatch2P;
        }

        vector<PairAlignerPtr> aligners;
        BackgroundMultiConsumer<pair<GossReadPtr, GossReadPtr> > grp(128);
        for (uint64_t i = 0; i < mNumThreads; ++i)
        {
            aligners.push_back(PairAlignerPtr(new PairAligner(g, m, match1Str, match2Str, 
                                                              nonMatch1Str, nonMatch2Str)));
            grp.add(*aligners.back());
        }

        log(info, "Filtering reads....");
        uint64_t n = 0;
        for (;reads.valid(); ++reads)
        {
            grp.push_back(make_pair(reads.lhs().clone(), reads.rhs().clone()));
            ++n;
        }
        grp.wait();
    }
    else
    {
        UnboundedProgressMonitor umon(log, 100000, " reads");
        LineSourceFactory lineSrcFac(BackgroundLineSource::create);
        ReadSequenceFileSequence reads(items, fac, lineSrcFac, &umon);

        FileFactory::OutHolderPtr matchP;
        ostream* match = NULL;
        if (mMatch.size())
        {
            matchP = fac.out(mMatch);
            match = &**matchP;
        }

        FileFactory::OutHolderPtr nonMatchP;
        ostream* nonMatch = NULL;
        if (mNonMatch.size())
        {
            nonMatchP = fac.out(mNonMatch);
            nonMatch = &**nonMatchP;
        }

        vector<ReadAlignerPtr> aligners;
        BackgroundMultiConsumer<GossReadPtr> grp(128);
        for (uint64_t i = 0; i < mNumThreads; ++i)
        {
            aligners.push_back(ReadAlignerPtr(new ReadAligner(g, m, match, nonMatch)));
            grp.add(*aligners.back());
        }

        log(info, "Filtering reads....");
        uint64_t n = 0;
        for (;reads.valid(); ++reads)
        {
            
            grp.push_back((*reads).clone());
            ++n;
        }
        grp.wait();
    }
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}

GossCmdPtr
GossCmdFactoryFilterReads::create(App& pApp, const boost::program_options::variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    strings fastas;
    chk.getRepeating0("fasta-in", fastas, readChk);

    strings fastqs;
    chk.getRepeating0("fastq-in", fastqs, readChk);

    strings lines;
    chk.getRepeating0("line-in", lines, readChk);

    bool pairs;
    chk.getOptional("pairs", pairs);

    bool count;
    chk.getOptional("count", count);

    string match;
    chk.getOptional("match-file", match);

    string nonmatch;
    chk.getOptional("non-match-file", nonmatch);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdFilterReads(in, fastas, fastqs, lines, pairs, count, T, match, nonmatch));
}

GossCmdFactoryFilterReads::GossCmdFactoryFilterReads()
    : GossCmdFactory("filter reads keeping/discarding those that coincide with a graph.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");

    mSpecificOptions.addOpt<string>("match-file", "",
            "a file to put matching reads into");
    mSpecificOptions.addOpt<string>("non-match-file", "",
            "a file to put non-matching reads into");
    mSpecificOptions.addOpt<bool>("pairs", "",
            "treat reads as pairs");
    mSpecificOptions.addOpt<bool>("count", "",
            "generate histogram of matching kmers per read");
}
