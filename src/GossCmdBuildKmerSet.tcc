// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#ifndef ASYNCMERGE_HH
#include "AsyncMerge.hh"
#endif

#ifndef BACKGROUNDMULTICONSUMER_HH
#include "BackgroundMultiConsumer.hh"
#endif

#ifndef BACKYARDHASH_HH
#include "BackyardHash.hh"
#endif

#ifndef EDGEANDCOUNT_HH
#include "EdgeAndCount.hh"
#endif

#ifndef KMERSET_HH
#include "KmerSet.hh"
#endif

#ifndef TIMER_HH
#include "Timer.hh"
#endif

#ifndef STD_STRING
#include <string>
#endif

#ifndef STD_UTILITY
#include <utility>
#define STD_UTILITY
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

namespace {

    typedef std::vector<Gossamer::edge_type> KmerBlock;
    typedef std::shared_ptr<KmerBlock> KmerBlockPtr;
    static const uint64_t blkSz = 1024;

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
        std::vector<uint32_t> perm;
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
                std::pair<Gossamer::edge_type,uint64_t> prev = pHash[perm[0]];
                for (uint32_t i = 1; i < perm.size(); ++i)
                {
                    std::pair<Gossamer::edge_type,uint64_t> itm = pHash[perm[i]];
                    //cerr << "edge: " << itm.first.value() << ", count: " << itm.second << '\n';
                    if ((itm.first < prev.first))
                    {
                        std::cerr << "ERROR! Edges out of order\n";
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
        pLog(info, "wrote " + boost::lexical_cast<std::string>(perm.size()) + " pairs.");
        return n;
    }


    void flush(const BackyardHash& pHash, uint64_t pK, const std::string& pGraphName,
               uint64_t pNumThreads, Logger& pLog, FileFactory& pFactory)
    {
        vector<uint32_t> perm;
        pLog(info, "sorting the hashtable...");
        pHash.sort(perm, pNumThreads);
        pLog(info, "sorting done.");

        try
        {
            KmerSet::Builder bld(pK, pGraphName, pFactory, perm.size());
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
}

template<typename KmerSrc> 
void 
GossCmdBuildKmerSet::operator()(const GossCmdContext& pCxt, KmerSrc& pKmerSrc)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);

    Timer t;
    log(info, "accumulating edges.");

    log(info, "using " + boost::lexical_cast<std::string>(mS) + " slot bits.");
    log(info, "using " + boost::lexical_cast<std::string>(log2(mN)) + " table bits.");

    BackyardHash h(mS, 2 * mK, mN);
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
    std::vector<std::string> parts;
    std::vector<uint64_t> sizes;
    std::string tmp = fac.tmpName();
    while (pKmerSrc.valid())
    {
        Gossamer::position_type kmer = *pKmerSrc;
        kmer.normalize(mK);
        blk->push_back(kmer);
        if (blk->size() == blkSz)
        {
            Profile::Context pc("GossCmdBuildKmerSet::push-block");
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
                    log(info, "processed " + boost::lexical_cast<std::string>(n * blkSz) + " individual k-mers.");
                    log(info, "hash table load is " + boost::lexical_cast<std::string>(ld));
                    log(info, "number of spills is " + boost::lexical_cast<std::string>(spl));
                    log(info, "the average k-mer frequency is " + boost::lexical_cast<std::string>(1.0 * (nSinceClear * blkSz) / sz));
                    prevLoad = l;
                }
                if (spl > 100)
                {
                    bg.sync(mT);
                    uint64_t cap = h.capacity();
                    uint64_t sz = h.size();
                    double ld = static_cast<double>(sz) / static_cast<double>(cap);
                    log(info, "hash table load at dumping is " + boost::lexical_cast<std::string>(ld));
                    std::string nm = tmp + "-" + boost::lexical_cast<std::string>(j++);
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
        ++pKmerSrc;
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
        flush(h, mK, mKmerSetName, mT, log, fac);
    }
    else
    {
        if (h.size() > 0)
        {
            std::string nm = tmp + "-" + boost::lexical_cast<std::string>(j++);
            log(info, "dumping temporary graph " + nm);
            uint64_t z0 = flushNaked(h, nm, mT, log, fac);
            z += z0;
            parts.push_back(nm);
            sizes.push_back(z0);
            h.clear();
            log(info, "done.");
        }
        
        log(info, "merging temporary graphs");

        AsyncMerge::merge<KmerSet>(parts, sizes, mKmerSetName, mK, z, mT, 65536, fac);

        for (uint64_t i = 0; i < parts.size(); ++i)
        {
            fac.remove(parts[i]);
        }
    }

    log(info, "finish graph build");
    log(info, "total build time: " + boost::lexical_cast<std::string>(t.check()));
}
