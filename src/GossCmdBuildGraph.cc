// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildGraph.hh"

#include "AsyncMerge.hh"
#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "BackyardHash.hh"
#include "Debug.hh"
#include "EdgeAndCount.hh"
#include "EdgeCompiler.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossamerException.hh"
#include "GossCmdLintGraph.hh"
#include "GossCmdMergeGraphs.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "Logger.hh"
#include "Profile.hh"
#include "ReadSequenceFileSequence.hh"
#include "ReverseComplementAdapter.hh"
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
    Debug lintAfterBuild("lint-after-build", "lint the graph, to check for symmetry, etc after building it.");

    typedef vector<Gossamer::edge_type> KmerBlock;
    typedef std::shared_ptr<KmerBlock> KmerBlockPtr;
    static const uint64_t blkSz = 1024;

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

    class BackyardConsumer
    {
    public:
        void push_back(const KmerBlockPtr& pBlk)
        {
            Profile::Context pc("BackyardConsumer::push_back");
            const KmerBlock& blk(*pBlk);
            for (uint64_t i = 0; i < blk.size(); ++i)
            {
                mHash.insert(blk[i]);
            }
        }

        void end()
        {
        }

        BackyardConsumer(BackyardHash& pHash)
            : mHash(pHash)
        {
        }

    private:
        BackyardHash& mHash;
    };

    class NakedGraph
    {
    public:
        class Builder
        {
        public:
            void push_back(const Gossamer::position_type& pEdge, const uint64_t& pCount)
            {
                Gossamer::EdgeAndCount itm(pEdge, pCount);
                EdgeAndCountCodec::encode(mOut, mPrevEdge, itm);
                mPrevEdge = itm.first;
            }

            void end()
            {
            }

            Builder(const std::string& pBaseName, FileFactory& pFactory)
                : mOutHolder(pFactory.out(pBaseName)), mOut(**mOutHolder),
                  mPrevEdge(0)
            {
            }

        private:
            FileFactory::OutHolderPtr mOutHolder;
            std::ostream& mOut;
            Gossamer::position_type mPrevEdge;
        };
    };

    uint64_t flushNaked(const BackyardHash& pHash, const std::string& pGraphName,
                        uint64_t pNumThreads, Logger& pLog, FileFactory& pFactory)
    {
        vector<uint32_t> perm;
        pLog(info, "sorting the hashtable...");
        pHash.sort(perm, pNumThreads);
        pLog(info, "sorting done.");
        pLog(info, "writing out naked edges.");

        uint32_t n = 0;
        try 
        {
            NakedGraph::Builder bld(pGraphName, pFactory);
            if (perm.size() > 0)
            {
                // Keep track of the previous edge/count pair,
                // since it is *possible* for the BackyardHash
                // to contain duplicates.
                pair<Gossamer::edge_type,uint64_t> prev = pHash[perm[0]];
                for (uint32_t i = 1; i < perm.size(); ++i)
                {
                    pair<Gossamer::edge_type,uint64_t> itm = pHash[perm[i]];
                    //cerr << "edge: " << itm.first.value() << ", count: " << itm.second << '\n';
                    if ((itm.first < prev.first))
                    {
                        cerr << "ERROR! Edges out of order\n";
                    }

                    if (itm.first == prev.first)
                    {
                        prev.second += itm.second;
                        continue;
                    }
                    bld.push_back(prev.first, prev.second);
                    ++n;
                    prev = itm;
                }
                bld.push_back(prev.first, prev.second);
                ++n;
            }
            bld.end();
        }
        catch (ios_base::failure& e)
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::write_error_info(pGraphName));
        }
        pLog(info, "wrote " + lexical_cast<string>(perm.size()) + " pairs.");
        return n;
    }

    void flush(const BackyardHash& pHash, uint64_t pK, const std::string& pGraphName,
               uint64_t pNumThreads, Logger& pLog, FileFactory& pFactory)
    {
        vector<uint32_t> perm;
        pLog(info, "sorting the hashtable...");
        pHash.sort(perm, pNumThreads);
        pLog(info, "sorting done.");
        pLog(info, "estimated number of edges is " + lexical_cast<string>(perm.size()));

        try 
        {
            Graph::Builder bld(pK, pGraphName, pFactory, perm.size());
            if (perm.size() > 0)
            {
                // Keep track of the previous edge/count pair,
                // since it is *possible* for the BackyardHash
                // to contain duplicates.
                pair<Gossamer::edge_type,uint64_t> prev = pHash[perm[0]];
                for (uint32_t i = 1; i < perm.size(); ++i)
                {
                    pair<Gossamer::edge_type,uint64_t> itm = pHash[perm[i]];
                    //cerr << "edge: " << itm.first.value() << ", count: " << itm.second << '\n';
                    if ((itm.first < prev.first))
                    {
                        cerr << "ERROR! Edges out of order\n";
                    }

                    if (itm.first == prev.first)
                    {
                        prev.second += itm.second;
                        continue;
                    }
                    bld.push_back(Gossamer::edge_type(prev.first), prev.second);
                    prev = itm;
                }
                bld.push_back(Gossamer::edge_type(prev.first), prev.second);
            }
            bld.end();
        }
        catch (ios_base::failure& e)
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::write_error_info(pGraphName));
        }
    }

} // namespace anonymous

void
GossCmdBuildGraph::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);

    const uint64_t rho = mK + 1;

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

    ReverseComplementAdapter x(reads, rho);

    Timer t;
    log(info, "accumulating edges.");

    log(info, "using " + lexical_cast<string>(mS) + " slot bits.");
    log(info, "using " + lexical_cast<string>(log2((double)mN)) + " table bits.");

    BackyardHash h(mS, 2 * rho, mN);
    BackyardConsumer bc(h);

    BackgroundMultiConsumer<KmerBlockPtr> bg(4096);
    for (uint64_t i = 0; i < mT; ++i)
    {
        bg.add(bc);
    }

    KmerBlockPtr blk(new KmerBlock);
    blk->reserve(blkSz);
    uint64_t n = 0;
    uint64_t nSinceClear = 0;
    uint64_t j = 0;
    uint64_t prevLoad = 0;
    const uint64_t m = (1 << (mS / 2)) - 1;
    uint64_t z = 0;
    vector<string> parts;
    vector<uint64_t> sizes;
    string tmp = fac.tmpName();
    while (x.valid())
    {
        blk->push_back(*x);
        if (blk->size() == blkSz)
        {
            Profile::Context pc("GossCmdBuildGraph::push-block");
            bg.push_back(blk);
            blk = KmerBlockPtr(new KmerBlock);
            blk->reserve(blkSz);
            ++nSinceClear;
            if ((++n & m) == 0)
            {
                uint64_t cap = h.capacity();
                uint64_t spl = h.spills();
                uint64_t sz = h.size();
                double ld = static_cast<double>(sz) / static_cast<double>(cap);
                uint64_t l = 200 * ld;
                if (l != prevLoad)
                {
                    log(info, "processed " + lexical_cast<string>(n * blkSz) + " individual rho-mers.");
                    log(info, "hash table load is " + lexical_cast<string>(ld));
                    log(info, "number of spills is " + lexical_cast<string>(spl));
                    log(info, "the average rho-mer frequency is " + lexical_cast<string>(1.0 * (nSinceClear * blkSz) / sz));
                    prevLoad = l;
                }
                if (spl > 0)
                {
                    bg.sync(mT);
                    uint64_t cap = h.capacity();
                    uint64_t sz = h.size();
                    double ld = static_cast<double>(sz) / static_cast<double>(cap);
                    log(info, "hash table load at dumping is " + lexical_cast<string>(ld));
                    string nm = tmp + "-" + lexical_cast<string>(j++);
                    log(info, "dumping temporary graph " + nm);
                    uint64_t z0 = flushNaked(h, nm, mT, log, fac);
                    z += z0;
                    parts.push_back(nm);
                    sizes.push_back(z0);
                    h.clear();
                    nSinceClear = 0;
                    log(info, "done.");
                }
            }
        }
        ++x;
    }
    if (blk->size() > 0)
    {
        bg.push_back(blk);
        blk = KmerBlockPtr();
    }
    bg.wait();

    if (parts.size() == 0)
    {
        log(info, "writing out graph (no merging necessary).");
        flush(h, mK, mGraphName, mT, log, fac);
    }
    else
    {
        if (h.size() > 0)
        {
            string nm = tmp + "-" + lexical_cast<string>(j++);
            log(info, "dumping temporary graph " + nm);
            uint64_t z0 = flushNaked(h, nm, mT, log, fac);
            z += z0;
            parts.push_back(nm);
            sizes.push_back(z0);
            h.clear();
            log(info, "done.");
        }
        
        log(info, "merging temporary graphs");
        log(info, "estimated number of edges " + lexical_cast<string>(z));

        AsyncMerge::merge<Graph>(parts, sizes, mGraphName, mK, z, mT, 65536, fac);

        for (uint64_t i = 0; i < parts.size(); ++i)
        {
            fac.remove(parts[i]);
        }
    }

    log(info, "finish graph build");
    log(info, "total build time: " + lexical_cast<string>(t.check()));
    if (lintAfterBuild.on())
    {
        log(info, "linting graph now...");
        GossCmdLintGraph lint(mGraphName);
        lint(pCxt);
    }
}

GossCmdPtr
GossCmdFactoryBuildGraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    uint64_t K = 0;
    chk.getMandatory("kmer-size", K, GossOptionChecker::RangeCheck(Graph::MaxK));

    uint64_t B = 2;
    chk.getOptional("buffer-size", B);

    if (B > 24)
    {
        pApp.logger()(warning, "Unsupported --buffer-size " + lexical_cast<string>(B) + ", truncating to 24.");
        B = 24;
    }

    uint64_t S = BackyardHash::maxSlotBits(B << 30);

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
    strings fastaNameFiles;
    chk.getOptional("fastas-in", fastaNameFiles);
    chk.expandFilenames(fastaNameFiles, fastaNames, fac);

    strings fastqNames;
    chk.getRepeating0("fastq-in", fastqNames, readChk);
    strings fastqNameFiles;
    chk.getOptional("fastqs-in", fastqNameFiles);
    chk.expandFilenames(fastqNameFiles, fastqNames, fac);

    strings lineNames;
    chk.getRepeating0("line-in", lineNames, readChk);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildGraph(K, S, N, T, graphName, fastaNames, fastqNames, lineNames));
}

GossCmdFactoryBuildGraph::GossCmdFactoryBuildGraph()
    : GossCmdFactory("create a new graph")
{
    mCommonOptions.insert("kmer-size");
    mCommonOptions.insert("buffer-size");
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("fastas-in");
    mCommonOptions.insert("fastqs-in");
}
