// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdPoolSamples.hh"

#include "BackyardHash.hh"
#include "LineSource.hh"
#include "Debug.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdBuildKmerSet.hh"
#include "GossCmdMergeKmerSets.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "KmerSet.hh"
#include "LineParser.hh"
#include "RankSelect.hh"
#include "Timer.hh"

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <utility>
#include <memory>

using namespace boost;
using namespace boost::program_options;
using namespace std;
using namespace Gossamer;

typedef vector<string> strings;

namespace {

    class SimilarityCalc 
    {
    public:
 
        typedef std::tuple<uint64_t, uint64_t, double*> Arg;

        void push_back(Arg pArg)
        {
            const uint64_t i = std::get<0>(pArg);
            const uint64_t j = std::get<1>(pArg);
            double& result = *std::get<2>(pArg);

            uint64_t unionSz = 0;
            uint64_t interSz = 0;

            const uint64_t ofs_i = i * mStride;
            const uint64_t ofs_j = j * mStride;
            const uint64_t end_i = mSampleRanks[i + 1];
            const uint64_t end_j = mSampleRanks[j + 1];
            uint64_t rnk_i = mSampleRanks[i];
            uint64_t rnk_j = mSampleRanks[j];

            while (rnk_i < end_i && rnk_j < end_j)
            {
                const uint64_t pos_i = mArr.select(rnk_i).asUInt64() - ofs_i;
                const uint64_t pos_j = mArr.select(rnk_j).asUInt64() - ofs_j;
                if (pos_i == pos_j)
                {
                    unionSz += 1;
                    interSz += 1;
                    ++rnk_i;
                    ++rnk_j;
                }
                else if (pos_i < pos_j)
                {
                    unionSz += 1;
                    ++rnk_i;
                }
                else if (pos_j < pos_i)
                {
                    unionSz += 1;
                    ++rnk_j;
                }
            }
            unionSz += end_i - rnk_i;
            unionSz += end_j - rnk_j;

            result = double(interSz) / double(unionSz);
            std::unique_lock<std::mutex> l(mMutex);
            mMon.tick(++mTicks);
        }
        
        SimilarityCalc(const SparseArray& pArr, uint64_t pStride, 
                       const vector<uint64_t>& pSampleRanks, ProgressMonitorNew& pMon)
            : mArr(pArr), mStride(pStride), mSampleRanks(pSampleRanks), mMon(pMon), mTicks(0), mMutex()
        {
        }

    private:
        const SparseArray& mArr;
        const uint64_t mStride;
        const vector<uint64_t>& mSampleRanks;
        ProgressMonitorNew& mMon;
        uint64_t mTicks;
        std::mutex mMutex;
    };

    typedef std::shared_ptr<SimilarityCalc>  SimilarityCalcPtr;


}   // namespace anonymous

void
GossCmdPoolSamples::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    CmdTimer t(log);
    
    string outPrefix = mOutPrefix.empty() ? fac.tmpName() : mOutPrefix;

    vector<string> samples;

    // Include existing k-mer sets.
    for (uint64_t i = 0; i < mKmerSets.size(); ++i)
    {
        log(info, "including k-mer set " + mKmerSets[i]);
        samples.push_back(mKmerSets[i]);
    }

    // Build a k-mer set for each input file
    // TODO: handle pairs of input files!
    for (uint64_t i = 0; i < mFastaNames.size(); ++i)
    {
        log(info, "building k-mer set of " + mFastaNames[i]);
        string out = outPrefix + "-a" + lexical_cast<string>(i);
        samples.push_back(out);
        GossCmdBuildKmerSet(mK, mS, mN, mT, out, strings(1, mFastaNames[i]), strings(), strings())(pCxt);
    }
    for (uint64_t i = 0; i < mFastqNames.size(); ++i)
    {
        log(info, "building k-mer set of " + mFastqNames[i]);
        string out = outPrefix + "-q" + lexical_cast<string>(i);
        samples.push_back(out);
        GossCmdBuildKmerSet(mK, mS, mN, mT, out, strings(), strings(1, mFastqNames[i]), strings())(pCxt);
    }

    if (samples.empty())
    {
        log(info, "no samples");
        return;
    }
    
    // Build the union.
    log(info, "merging sets");
    string unionSetName = outPrefix + "-union";
    {
        GossCmdMergeKmerSets(samples, 8, unionSetName)(pCxt);
    }

    // Count the number of bits set in the (sample x k-mer) array,
    // and the rank of the beginning of each sample's bit vector 
    // (plus the rank of a bit one past the end.)
    uint64_t m = 0;
    vector<uint64_t> sampleRanks(1, 0);
    sampleRanks.reserve(samples.size());
    for (uint64_t i = 0; i < samples.size(); ++i)
    {
        KmerSet::LazyIterator itr(samples[i], fac);
        m += itr.count();
        sampleRanks.push_back(m);
    }

    // Construct the (sample x k-mer) bit array.
    const KmerSet unionSet(unionSetName, fac);
    const uint64_t z = unionSet.count();
    const position_type n = position_type(z * samples.size());
    const string kmerIndexName = outPrefix + "-index";
    {
        SparseArray::Builder bld(kmerIndexName, fac, n, m);
        log(info, lexical_cast<string>(z) + " " + lexical_cast<string>(unionSet.K()) + "-mers in union");
        for (uint64_t i = 0; i < samples.size(); ++i)
        {
            log(info, "constructing bit array for " + samples[i]);
            KmerSet::Iterator unionItr(unionSet);
            KmerSet::LazyIterator sampleItr(samples[i], fac);
            uint64_t rank = i * z;
            while (sampleItr.valid())
            {
                while ((*unionItr).first < (*sampleItr).first)
                {
                    // Catch up with the sample.
                    // Note: We assume a large union/sample intersection, and 
                    //       hence relatively few iterations of this loop.
                    ++rank;
                    ++unionItr;
                }
                bld.push_back(position_type(rank));
                ++rank;
                ++unionItr;
                ++sampleItr;
            }
        }
        bld.end(n);
    }

    // Build the pairwise similarity matrix.
    log(info, "calculating similarity matrix");
    const SparseArray kmerIndex(kmerIndexName, fac);
    vector<vector<double> > sim(samples.size(), vector<double>(samples.size()));
    BackgroundMultiConsumer<SimilarityCalc::Arg> grp(128);
    const uint64_t steps = samples.size() * (samples.size() - 1) / 2;
    ProgressMonitorNew mon(log, steps);
    SimilarityCalc calc(kmerIndex, z, sampleRanks, mon);
    for (uint64_t i = 0; i < mT; ++i)
    {
        grp.add(calc);
    }
    for (uint64_t i = 0; i < samples.size(); ++i)
    {
        sim[i][i] = 1.0;
        for (uint64_t j = i+1; j < samples.size(); ++j)
        {
            // log(info, "calculating similarity between " + samples[i] + " " + samples[j]);
            grp.push_back(SimilarityCalc::Arg(i, j, &sim[i][j]));
        }
    }
    grp.wait();

    FileFactory::OutHolderPtr matP(fac.out(mOutPrefix + ".sim"));
    ostream& mat (**matP);
    for (uint64_t i = 0; i < samples.size(); ++i)
    {
        for (uint64_t j = i+1; j < samples.size(); ++j)
        {
            mat << i << '\t' << j << '\t' << sim[i][j] << '\n';
        }
    }
}


GossCmdPtr
GossCmdFactoryPoolSamples::create(App& pApp, const variables_map& pOpts)
{
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker chk(pOpts);

    uint64_t K = 25;
    chk.getOptional("kmer-size", K, GossOptionChecker::RangeCheck(KmerSet::MaxK));

    uint64_t B = 2;
    chk.getOptional("buffer-size", B);
    uint64_t N = (B << 30) / (1.5 * sizeof(uint32_t) + sizeof(BackyardHash::value_type));
    uint64_t S = BackyardHash::maxSlotBits(B << 30);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);
    GossOptionChecker::FileReadCheck readChk(fac);
    GossOptionChecker::FileCreateCheck createChk(fac, true);

    string out;
    chk.getOptional("graph-out", out, createChk);

    strings kmerSetNames;
    chk.getRepeating0("kmer-set-in", kmerSetNames);

    strings kmerSetFiles;
    chk.getRepeating0("kmer-sets-in", kmerSetFiles);
    chk.expandFilenames(kmerSetFiles, kmerSetNames, fac);

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

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdPoolSamples(K, S, N, T, out, fastaNames, fastqNames, kmerSetNames));
}

GossCmdFactoryPoolSamples::GossCmdFactoryPoolSamples()
    : GossCmdFactory("pool all the samples")
{
    mCommonOptions.insert("kmer-size");
    mCommonOptions.insert("buffer-size");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastas-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("fastqs-in");
    mCommonOptions.insert("graph-out");

    mSpecificOptions.addOpt<strings>("kmer-set-in", "", "input k-mer set");
    mSpecificOptions.addOpt<strings>("kmer-sets-in", "", "input file containing k-mer sets");
}
