// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildKmerSet.hh"

#include "AsyncMerge.hh"
#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "BackyardHash.hh"
#include "Debug.hh"
#include "EdgeAndCount.hh"
#include "EdgeCompiler.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdLintGraph.hh"
#include "GossCmdMergeGraphs.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "KmerSet.hh"
#include "KmerizingAdapter.hh"
#include "LineParser.hh"
#include "Logger.hh"
#include "Profile.hh"
#include "ReadSequenceFileSequence.hh"
#include "Timer.hh"
#include "VByteCodec.hh"

#include <iostream>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace // anonymous
{
    class PauseButton
    {
    public:
        void checkPoint()
        {
            unique_lock<mutex> lk(mMutex);
            if (mPause)
            {
                --mArrived;
                if (mArrived == 0)
                {
                    mAllPausedCond.notify_one();
                }
                mPauseCond.wait(lk);
            }
        }

        void pause(uint64_t pHowMany)
        {
            unique_lock<mutex> lk(mMutex);
            mPause = true;
            mArrived = pHowMany;
            mAllPausedCond.wait(lk);
        }

        void resume()
        {
            unique_lock<mutex> lk(mMutex);
            mPause = false;
            mPauseCond.notify_all();
        }

        PauseButton()
            : mPause(false), mArrived(0)
        {
        }
    private:
        mutex mMutex;
        condition_variable mPauseCond;
        condition_variable mAllPausedCond;
        bool mPause;
        uint64_t mArrived;
    };

    class Pause
    {
    public:
        Pause(PauseButton& pButton, uint64_t pHowMany)
            : mButton(pButton)
        {
            mButton.pause(pHowMany);
        }

        ~Pause()
        {
            mButton.resume();
        }

    private:
        PauseButton& mButton;
    };

} // namespace anonymous



void
GossCmdBuildKmerSet::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);

    std::deque<GossReadSequence::Item> items;

    {
        GossReadSequenceFactoryPtr seqFac
            = std::make_shared<GossReadSequenceBasesFactory>();

        GossReadParserFactory lineParserFac(LineParser::create);
        for (auto& f: mLineNames)
        {
            items.push_back(GossReadSequence::Item(f, lineParserFac, seqFac));
        }

        GossReadParserFactory fastaParserFac(FastaParser::create);
        for (auto& f: mFastaNames)
        {
            items.push_back(GossReadSequence::Item(f, fastaParserFac, seqFac));
        }

        GossReadParserFactory fastqParserFac(FastqParser::create);
        for (auto& f: mFastqNames)
        {
            items.push_back(GossReadSequence::Item(f, fastqParserFac, seqFac));
        }
    }

    UnboundedProgressMonitor umon(log, 100000, " reads");
    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    ReadSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);

    KmerizingAdapter x(reads, mK);
    (*this)(pCxt, x);
}

GossCmdPtr
GossCmdFactoryBuildKmerSet::create(App& pApp, const variables_map& pOpts)
{

    GossOptionChecker chk(pOpts);

    uint64_t K = 0;
    chk.getMandatory("kmer-size", K, GossOptionChecker::RangeCheck(KmerSet::MaxK));

    uint64_t B = 2;
    chk.getOptional("buffer-size", B);

    uint64_t S = BackyardHash::maxSlotBits(B << 30);
    chk.getOptional("log-hash-slots", S);

    uint64_t N = (B << 30) / (1.5 * sizeof(uint32_t) + sizeof(BackyardHash::value_type));

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileCreateCheck createChk(fac, true);
    GossOptionChecker::FileReadCheck readChk(fac);

    string graphName;
    chk.getMandatory("graph-out", graphName, createChk);

    strings fastaNames;
    chk.getRepeating0("fasta-in", fastaNames, readChk);

    strings fasFiles;
    chk.getOptional("fastas-in", fasFiles);
    chk.expandFilenames(fasFiles, fastaNames, fac);

    strings fastqNames;
    chk.getRepeating0("fastq-in", fastqNames, readChk);

    strings fqsFiles;
    chk.getOptional("fastqs-in", fqsFiles);
    chk.expandFilenames(fqsFiles, fastqNames, fac);

    strings lineNames;
    chk.getRepeating0("line-in", lineNames, readChk);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildKmerSet(K, S, N, T, graphName, fastaNames, fastqNames, lineNames));
}

GossCmdFactoryBuildKmerSet::GossCmdFactoryBuildKmerSet()
    : GossCmdFactory("create a new graph")
{
    mCommonOptions.insert("kmer-size");
    mCommonOptions.insert("buffer-size");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastas-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("fastqs-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("log-hash-slots");

    mSpecificOptions.addOpt<uint64_t>("log-hash-slots", "S",
            "log2 of the number of hash slots to use (default 24)");
}
