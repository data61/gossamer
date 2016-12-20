// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "EspressoApp.hh"

#include "Debug.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossOption.hh"
#include "GossReadDispatcher.hh"
#include "GossReadProcessor.hh"
#include "GossVersion.hh"
#include "KmerSpectrum.hh"
#include "Logger.hh"
#include "MultithreadedBatchTask.hh"
#include "PhysicalFileFactory.hh"
#include "SimpleHashMap.hh"
#include "SimpleHashSet.hh"
#include "Timer.hh"

#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>

#include <thread>
#include <random>

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

    class EspressoCmdSingle : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            Logger& log(pCxt.log);

            log(info, "constructing single kmer spectrum");
            GossReadHandlerPtr p = KmerSpectrum::singleRowBuilder(mK, mVar, mFileName);
            GossReadProcessor::processSingle(pCxt, mFastas, mFastqs, mLines, *p);
            p->end();

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        EspressoCmdSingle(const strings& pFastas, const strings& pFastqs, const strings& pLines, const string& pVar, const string& pFileName, uint64_t pK)
            : mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), mVar(pVar), mFileName(pFileName), mK(pK)
        {
        }

    private:
        const strings mFastas;
        const strings mFastqs;
        const strings mLines;
        const string mVar;
        const string mFileName;
        const uint64_t mK;
    };

    class EspressoCmdFactorySingle : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            uint64_t K = 8;
            chk.getOptional("kmer-size", K);

            strings fas;
            chk.getOptional("fasta-in", fas);

            strings fqs;
            chk.getOptional("fastq-in", fqs);

            strings ls;
            chk.getOptional("line-in", ls);

            string v("v");
            chk.getOptional("var", v);

            string m("m.mat");
            chk.getOptional("matrix-out", m);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new EspressoCmdSingle(fas, fqs, ls, v, m, K));
        }

        EspressoCmdFactorySingle()
            : GossCmdFactory("construct an aggregate kmer spectrum from a collection of sequences")
        {
            mCommonOptions.insert("kmer-size");
            mCommonOptions.insert("fasta-in");
            mCommonOptions.insert("fastq-in");
            mCommonOptions.insert("line-in");
            mCommonOptions.insert("var");
            mCommonOptions.insert("matrix-out");
        }
    };

    class EspressoCmdSparseSingle : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            FileFactory& fac(pCxt.fac);
            Logger& log(pCxt.log);

            log(info, "constructing a matrix of kmer spectra");
            KmerSet kmerSet(mKmerSet, fac);
            GossReadHandlerPtr p = KmerSpectrum::sparseSingleRowBuilder(kmerSet, mKmerSet, mVar, mFileName);
            GossReadProcessor::processSingle(pCxt, mFastas, mFastqs, mLines, *p);
            //p->end();

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        EspressoCmdSparseSingle(const string& pKmerSet, const strings& pFastas, const strings& pFastqs, const strings& pLines, const string& pVar, const string& pFileName)
            : mKmerSet(pKmerSet), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), mVar(pVar), mFileName(pFileName)
        {
        }

    private:
        const string mKmerSet;
        const strings mFastas;
        const strings mFastqs;
        const strings mLines;
        const string mVar;
        const string mFileName;
    };

    class EspressoCmdFactorySparseSingle : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            string g;
            chk.getRepeatingOnce("graph-in", g);

            strings fas;
            chk.getOptional("fasta-in", fas);

            strings fqs;
            chk.getOptional("fastq-in", fqs);

            strings ls;
            chk.getOptional("line-in", ls);

            string v("v");
            chk.getOptional("var", v);

            string m("m.mat");
            chk.getOptional("matrix-out", m);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new EspressoCmdSparseSingle(g, fas, fqs, ls, v, m));
        }

        EspressoCmdFactorySparseSingle()
            : GossCmdFactory("construct a matrix of kmer spectra from a collection of sequences")
        {
            mCommonOptions.insert("graph-in");
            mCommonOptions.insert("fasta-in");
            mCommonOptions.insert("fastq-in");
            mCommonOptions.insert("line-in");
            mCommonOptions.insert("var");
            mCommonOptions.insert("matrix-out");
        }
    };

    class EspressoCmdMulti : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            Logger& log(pCxt.log);

            log(info, "constructing a matrix of kmer spectra");
            GossReadHandlerPtr p = KmerSpectrum::multiRowBuilder(mK, mVar, mFileName);
            GossReadProcessor::processSingle(pCxt, mFastas, mFastqs, mLines, *p);
            //p->end();

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        EspressoCmdMulti(const strings& pFastas, const strings& pFastqs, const strings& pLines, const string& pVar, const string& pFileName, uint64_t pK)
            : mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), mVar(pVar), mFileName(pFileName), mK(pK)
        {
        }

    private:
        const strings mFastas;
        const strings mFastqs;
        const strings mLines;
        const string mVar;
        const string mFileName;
        const uint64_t mK;
    };

    class EspressoCmdFactoryMulti : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            uint64_t K = 8;
            chk.getOptional("kmer-size", K);

            strings fas;
            chk.getOptional("fasta-in", fas);

            strings fqs;
            chk.getOptional("fastq-in", fqs);

            strings ls;
            chk.getOptional("line-in", ls);

            string v("v");
            chk.getOptional("var", v);

            string m("m.mat");
            chk.getOptional("matrix-out", m);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new EspressoCmdMulti(fas, fqs, ls, v, m, K));
        }

        EspressoCmdFactoryMulti()
            : GossCmdFactory("construct a matrix of kmer spectra from a collection of sequences")
        {
            mCommonOptions.insert("kmer-size");
            mCommonOptions.insert("fasta-in");
            mCommonOptions.insert("fastq-in");
            mCommonOptions.insert("line-in");
            mCommonOptions.insert("var");
            mCommonOptions.insert("matrix-out");
        }
    };

    class EspressoCmdSparseMulti : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            FileFactory& fac(pCxt.fac);
            Logger& log(pCxt.log);

            log(info, "constructing a matrix of kmer spectra");
            KmerSet kmerUniv(mKmerUniv, fac);
            GossReadHandlerPtr p = KmerSpectrum::sparseMultiRowBuilder(kmerUniv, mKmerUniv, mKmerSets, false, fac);
            GossReadProcessor::processSingle(pCxt, mFastas, mFastqs, mLines, *p);

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        EspressoCmdSparseMulti(const string& pKmerSet, const strings& pFastas, const strings& pFastqs, const strings& pLines, const strings& pKmerSets,
                               const string& pVar, const string& pFileName)
            : mKmerUniv(pKmerSet), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), mKmerSets(pKmerSets),
              mVar(pVar), mFileName(pFileName)
        {
        }

    private:
        const string mKmerUniv;
        const strings mFastas;
        const strings mFastqs;
        const strings mLines;
        const strings mKmerSets;
        const string mVar;
        const string mFileName;
    };

    class EspressoCmdFactorySparseMulti : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            string g;
            chk.getRepeatingOnce("graph-in", g);

            strings fas;
            chk.getOptional("fasta-in", fas);

            strings fasFiles;
            chk.getOptional("fastas-in", fasFiles);
            chk.expandFilenames(fasFiles, fas, fac);

            strings fqs;
            chk.getOptional("fastq-in", fqs);

            strings fqsFiles;
            chk.getOptional("fastqs-in", fqsFiles);
            chk.expandFilenames(fqsFiles, fqs, fac);

            strings ls;
            chk.getOptional("line-in", ls);

            strings kms;
            chk.getOptional("kmer-set-in", kms);

            strings kmsFiles;
            chk.getOptional("kmer-sets-in", kmsFiles);
            chk.expandFilenames(kmsFiles, kms, fac);

            string v("v");
            chk.getOptional("var", v);

            string m("m.mat");
            chk.getOptional("matrix-out", m);


            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new EspressoCmdSparseMulti(g, fas, fqs, ls, kms, v, m));
        }

        EspressoCmdFactorySparseMulti()
            : GossCmdFactory("construct a matrix of kmer spectra from a collection of sequences")
        {
            mCommonOptions.insert("graph-in");
            mCommonOptions.insert("fasta-in");
            mCommonOptions.insert("fastas-in");
            mCommonOptions.insert("fastq-in");
            mCommonOptions.insert("fastqs-in");
            mCommonOptions.insert("line-in");
            mCommonOptions.insert("var");
            mCommonOptions.insert("matrix-out");
            mCommonOptions.insert("kmer-set-in");
            mCommonOptions.insert("kmer-sets-in");
        }
    };

    string disp(const dynamic_bitset<>& pBitmap)
    {
        string r;
        for (uint64_t i = 0; i < pBitmap.size(); ++i)
        {
            r.push_back("01"[pBitmap[i]]);
        }
        return r;
    }

    double logAdd(double pLogX, double pLogY)
    {
        double x = max(pLogX, pLogY);
        double y = min(pLogX, pLogY);
        return x + log1p(exp(y - x));
    }

    double logSub(double pLogX, double pLogY)
    {
        double x = pLogX;
        double y = pLogY;
        return x - log1p(exp(y - x));
    }

    double logBinEq0(double pLogP, double pLog1mP, uint64_t pN, uint64_t pK)
    {
        return Gossamer::logChoose(pN, pK) + pLogP * pK + pLog1mP * (pN - pK);
    }

    double logBinEq(double pP, uint64_t pN, uint64_t pK)
    {
        return logBinEq0(log(pP), log(1 - pP), pN, pK);
    }

    double logBinGe(double pP, uint64_t pN, uint64_t pK)
    {
        double lp = log(pP);
        double l1mp = log(1 - pP);
        double r = logBinEq0(lp, l1mp, pN, pK);
        for (uint64_t i = pK + 1; i <= pN; ++i)
        {
            r = logAdd(r, logBinEq0(lp, l1mp, pN, i));
        }
        return r;
    }

    class Writer
    {
    public:
        virtual void operator()(bool pHit, uint64_t pGenNum, double pScore) = 0;

        virtual ~Writer() {}
    };

    class SingleWriter : public Writer
    {
    public:
        void operator()(bool pHit, uint64_t pGeneNum, double pScore)
        {
            if (pHit && mHit)
            {
                (*mHit) << pGeneNum << '\t'
                    << static_cast<int>(-log(pScore)) << '\t'
                    << mRead.read() << '\t'
                    << mRead.qual() << '\n';
            }
            else if (!pHit && mMiss)
            {
                (*mHit) << mRead.read() << '\t'
                    << mRead.qual() << '\n';
            }
        }

        SingleWriter(ostream* pHit, ostream* pMiss, const GossRead& pRead)
            : mHit(pHit), mMiss(pMiss), mRead(pRead)
        {
        }

    private:
        ostream* mHit;
        ostream* mMiss;
        const GossRead& mRead;
    };

    class PairWriter : public Writer
    {
    public:
        void operator()(bool pHit, uint64_t pGeneNum, double pScore)
        {
            if (pHit && mHit)
            {
                (*mHit) << pGeneNum << '\t'
                     << static_cast<int>(-log(pScore)) << '\t'
                     << mLhs.read() << '\t'
                     << mLhs.qual() << '\t'
                     << mRhs.read() << '\t'
                     << mRhs.qual() << '\n';
            }
            else if (!pHit && mMiss)
            {
                (*mMiss) << mLhs.read() << '\t'
                     << mLhs.qual() << '\t'
                     << mRhs.read() << '\t'
                     << mRhs.qual() << '\n';
            }
        }

        PairWriter(ostream* pHit, ostream* pMiss, const GossRead& pLhs, const GossRead& pRhs)
            : mHit(pHit), mMiss(pMiss), mLhs(pLhs), mRhs(pRhs)
        {
        }

    private:
        ostream* mHit;
        ostream* mMiss;
        const GossRead& mLhs;
        const GossRead& mRhs;
    };

    class QueryProcessor : public GossReadHandler
    {
        typedef SimpleHashMap<uint64_t, uint64_t> Hits;         // Sample -> Count
        typedef SimpleHashSet<Gossamer::position_type> Kmers;

    public:
        typedef pair<uint64_t,uint64_t> GenePair;

        vector<double> counts;
        uint64_t readCount;

        void operator()(const GossRead& pRead)
        {
            Hits hits;
            Kmers kmers;
            processRead(pRead, hits, kmers);
            SingleWriter w(mClassifiedFile, mUnclassifiedFile, pRead);
            updateCounts(hits, kmers.size(), w);
        }

        void operator()(const GossRead& pLhs, const GossRead& pRhs)
        {
            Hits hits;
            Kmers kmers;
            processRead(pLhs, hits, kmers);
            processRead(pRhs, hits, kmers);

            PairWriter w(mClassifiedFile, mUnclassifiedFile, pLhs, pRhs);
            updateCounts(hits, kmers.size(), w);
        }

        QueryProcessor(const KmerSet& pKmers, const SparseArray& pIdx, const MappedArray<uint64_t>& pLens,
                       ostream* pClassifiedFile, ostream* pUnclassifiedFile)
            : readCount(0), mMutex(), mKmers(pKmers), mIdx(pIdx), mLens(pLens),
              mRho(pKmers.K()), mNumGenes(mIdx.size().asUInt64()/mKmers.count()),
              mClassifiedFile(pClassifiedFile),
              mUnclassifiedFile(pUnclassifiedFile),
              mRng(17)
        {
            counts.resize(mNumGenes, -log(mNumGenes));
        }

    private:

        void processRead(const GossRead& pRead, Hits& pHits, Kmers& pKmers) const
        {
            for (GossRead::Iterator i(pRead, mRho); i.valid(); ++i)
            {
                Gossamer::position_type x = i.kmer();
                x.normalize(mRho);
                if (pKmers.count(x))
                {
                    continue;
                }
                pKmers.insert(x);
                uint64_t rnk = 0;
                if (mKmers.accessAndRank(KmerSet::Edge(x), rnk))
                {
                    const uint64_t n = mIdx.size().asUInt64() / mKmers.count();
                    Gossamer::position_type a(rnk * n);
                    Gossamer::position_type b((rnk + 1) * n);
                    pair<uint64_t,uint64_t> r = mIdx.rank(a, b);
                    for (uint64_t j = r.first; j < r.second; ++j)
                    {
                        Gossamer::position_type s = mIdx.select(j);
                        uint64_t g = s.asUInt64() - a.asUInt64();
                        pHits[g] += 1;
                    }
                }
            }
            //pRead.print(cerr);
        }

        void updateCounts(const Hits& pHits, uint64_t pNumKmers, Writer& pWriter)
        {
            if (pHits.size() == 0)
            {
                boost::unique_lock<boost::mutex> lk(mMutex);
                pWriter(false, 0, 0);
                return;
            }


            double eps = 0.01;
            //double logEps = log(eps);
            //double logHit = log1p(-eps);
            vector<pair<uint64_t,double> > scores;
            vector<pair<double,uint64_t> > classes;
            scores.reserve(pHits.size());
            //cout << pNumKmers;
            for (SimpleHashMap<uint64_t,uint64_t>::Iterator i(pHits); i.valid(); ++i)
            {
                pair<uint64_t,uint64_t> j = *i;
                uint64_t l = mLens[j.first];
                uint64_t m = l - j.second;
                //cout << '\t' << j.first << ':' << j.second;
                //double logP = log(mLens[j.first]);
                //double hitPart = logHit * j.second;
                //double epsPart = logEps * (pNumKmers - j.second);
                //double logScore = hitPart + epsPart - logP;
                double logScore = logBinGe(eps, l, m);
                scores.push_back(pair<uint64_t,double>(j.first, logScore));
                classes.push_back(pair<double,uint64_t>(-logScore, j.first));
            }
            //cout << endl;

            sort(classes.begin(), classes.end());
            vector<pair<uint64_t,double> > family;
            for (uint64_t i = 0; i < classes.size(); ++i)
            {
                if (classes[i].first > classes[0].first + 5 || classes[i].first > 100)
                {
                    break;
                }
                family.push_back(pair<uint64_t,double>(classes[i].second, classes[i].first));
            }
            sort(family.begin(), family.end());


            // TODO: Locking too coarse?
            boost::unique_lock<boost::mutex> lk(mMutex);

            if (0 && family.size())
            {
                for (uint64_t i = 0; i < family.size(); ++i)
                {
                    cerr << family[i].first;
                    if (i != family.size() - 1)
                    {
                        cerr << ':';
                    }
                }
                cerr << '\t';
                for (uint64_t i = 0; i < family.size(); ++i)
                {
                    cerr << family[i].second;
                    if (i != family.size() - 1)
                    {
                        cerr << ':';
                    }
                }
                cerr << '\n';
            }

            ++readCount;
            double logReadCount = log(readCount);

            scores[0]. second += counts[scores[0].first] - logReadCount;
            double logSum = scores[0].second;
            for (uint64_t i = 1; i < scores.size(); ++i)
            {
                scores[i]. second += counts[scores[i].first] - logReadCount;
                logSum = logAdd(logSum, scores[i].second);
            }
            double x = mDist(mRng);
            double cumu = 0;
            double written = false;
            for (uint64_t i = 0; i < scores.size(); ++i)
            {
                scores[i].second -= logSum;
                double s = logAdd(counts[scores[i].first], scores[i].second);
                counts[scores[i].first] = s;
                cumu += exp(scores[i].second);
                if (x < cumu && !written)
                {
                    pWriter(true, scores[i].first, exp(scores[i].second + logSum));
                    written = true;
                }
            }
        }

        mutex mMutex;
        const KmerSet& mKmers;
        const SparseArray& mIdx;
        const MappedArray<uint64_t>& mLens;
        const uint64_t mRho;
        const uint64_t mNumGenes;
        ostream* mClassifiedFile;
        ostream* mUnclassifiedFile;

        std::mt19937 mRng;
        std::uniform_real_distribution<> mDist;

    };

    class EspressoCmdDoQueries : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            FileFactory& fac(pCxt.fac);
            Logger& log(pCxt.log);

            Timer t;
            KmerSet ks(mKmersName, fac);
            cerr << "ks.count() = " << ks.count() << endl;
            SparseArray idx(mKmersName + ".idx", fac);
            //const uint64_t n = idx.size().asUInt64() / ks.count();

            MappedArray<uint64_t> lens(mKmersName + ".lens", fac);
            vector<string> names;
            {
                FileFactory::InHolderPtr inp = fac.in(mKmersName + ".names");
                istream& in = **inp;
                while (in.good())
                {
                    string nm;
                    getline(in, nm);
                    if (!in.good())
                    {
                        break;
                    }
                    names.push_back(nm);
                }
            }

            if (0)
            {
                map<uint64_t,uint64_t> hist;
                for (uint64_t i = 0; i < ks.count(); ++i)
                {
                    const uint64_t n = idx.size().asUInt64() / ks.count();
                    Gossamer::position_type a(i * n);
                    Gossamer::position_type b((i + 1) * n);
                    pair<uint64_t,uint64_t> r = idx.rank(a, b);
                    hist[r.second - r.first]++;
                }
                for (map<uint64_t,uint64_t>::const_iterator i = hist.begin(); i != hist.end(); ++i)
                {
                    cerr << i->first << '\t' << i->second << endl;
                }
                return;
            }

            ostream* classifiedFile = NULL;
            FileFactory::OutHolderPtr classifiedFileHolder;
            if (mClassifiedFilename.size())
            {
                classifiedFileHolder = fac.out(mClassifiedFilename);
                classifiedFile = &(**classifiedFileHolder);
            }

            ostream* unclassifiedFile = NULL;
            FileFactory::OutHolderPtr unclassifiedFileHolder;
            if (mUnclassifiedFilename.size())
            {
                unclassifiedFileHolder = fac.out(mUnclassifiedFilename);
                unclassifiedFile = &(**unclassifiedFileHolder);
            }

            shared_ptr<QueryProcessor> qpp(new QueryProcessor(ks, idx, lens, classifiedFile, unclassifiedFile));
            vector<shared_ptr<GossReadHandler> > qpps(mNumThreads, static_pointer_cast<GossReadHandler>(qpp));

            if (mPairs)
            {
                GossPairDispatcher qp(qpps);
                UnboundedProgressMonitor umon(log, 100000, " read pairs");
                GossReadProcessor::processPairs(pCxt, mFastas, mFastqs, mLines, qp, &umon, &log);
            }
            else
            {
                GossReadDispatcher qp(qpps);
                UnboundedProgressMonitor umon(log, 100000, " reads");
                GossReadProcessor::processSingle(pCxt, mFastas, mFastqs, mLines, qp, &umon, &log);
            }

            //cerr << qpp->counts.size() << '\t' << names.size() << endl;

            cout << "id" << '\t' << "logCount" << '\t' << "count" << '\t' << "countError" << '\t' << "fpkm" << '\t' << "fpkmError" << '\t' << "name" << endl;
            double rc = qpp->readCount;
            double m = 1e6 / rc;
            double den = 1.0 / (rc * rc * (rc + 1.0));
            for (uint64_t i = 0; i < qpp->counts.size(); ++i)
            {
                double k = 1e3 / lens[i];
                double c = exp(qpp->counts[i]);
                if (c < 1)
                {
                    continue;
                }
                double var = c * (rc - c) * den;
                double se = sqrt(var) * rc;
                cout << i << '\t' << qpp->counts[i] << '\t' << c << '\t' << se << '\t' << (c * k * m) << '\t' << (se * k * m) << '\t' << names[i] << endl;
            }

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        EspressoCmdDoQueries(const string& pKmersName,
                             bool pPairs,
                             const strings& pFastas,
                             const strings& pFastqs,
                             const strings& pLines,
                             const string& pClassifiedFilename,
                             const string& pUnclassifiedFilename,
                             uint64_t pNumThreads)
            : mKmersName(pKmersName), mPairs(pPairs), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines),
              mClassifiedFilename(pClassifiedFilename), mUnclassifiedFilename(pUnclassifiedFilename),
              mNumThreads(pNumThreads)
        {
        }

    private:
        const string mKmersName;
        const bool mPairs;
        const strings mFastas;
        const strings mFastqs;
        const strings mLines;
        const string mClassifiedFilename;
        const string mUnclassifiedFilename;
        const uint64_t mNumThreads;
    };

    class EspressoCmdFactoryDoQueries : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            string g;
            chk.getRepeatingOnce("graph-in", g);

            bool ps = false;
            chk.getOptional("pairs", ps);

            strings fas;
            chk.getOptional("fasta-in", fas);

            strings fqs;
            chk.getOptional("fastq-in", fqs);

            strings ls;
            chk.getOptional("line-in", ls);

            uint64_t T = 1;
            chk.getOptional("num-threads", T);

            string cf;
            chk.getOptional("classified-reads-file", cf);

            string ucf;
            chk.getOptional("unclassified-reads-file", ucf);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new EspressoCmdDoQueries(g, ps, fas, fqs, ls, cf, ucf, T));
        }

        EspressoCmdFactoryDoQueries()
            : GossCmdFactory("")
        {
            mCommonOptions.insert("graph-in");
            mCommonOptions.insert("pairs");
            mCommonOptions.insert("fasta-in");
            mCommonOptions.insert("fastq-in");
            mCommonOptions.insert("line-in");
            mCommonOptions.insert("classified-reads-file");
            mCommonOptions.insert("unclassified-reads-file");
        }
    };

    class EspressoCmdSimilarity : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            FileFactory& fac(pCxt.fac);
            Logger& log(pCxt.log);
            CmdTimer t(log);

            KmerSet ks(mKmersName, fac);
            SparseArray idx(mKmersName + ".idx", fac);
            //const uint64_t n = idx.size().asUInt64() / ks.count();

            uint64_t sIx = -1;
            MappedArray<uint64_t> lens(mKmersName + ".lens", fac);
            vector<string> names;
            {
                FileFactory::InHolderPtr inp = fac.in(mKmersName + ".names");
                istream& in = **inp;
                uint64_t ix = 0; 
                while (in.good())
                {
                    string nm;
                    getline(in, nm);
                    if (!in.good())
                    {
                        break;
                    }
                    names.push_back(nm);
                    if (nm.find(mSampleName) != string::npos)
                    {
                        sIx = ix;
                    }
                    ++ix;
                }
            }

            if (sIx == uint64_t(-1))
            {
                log(::error, "no sample named '" + mSampleName + "'");
                return;
            }

            log(info, "sample index " + lexical_cast<string>(sIx));

            const uint64_t numSamples(names.size());
            const uint64_t numKmers(ks.count());
            vector<uint64_t> unions(numSamples);
            vector<uint64_t> ints(numSamples);
            ProgressMonitorNew mon(log, numKmers);
            for (uint64_t i = 0; i < numKmers; ++i)
            {
                mon.tick(i);
                bool inSample = idx.access(Gossamer::position_type(i * numSamples + sIx));
                Gossamer::position_type a(i * numSamples);
                Gossamer::position_type b((i + 1) * numSamples);
                pair<Gossamer::rank_type, Gossamer::rank_type> rs(idx.rank(a, b));
                for (uint64_t r = rs.first; r < rs.second; ++r)
                {   
                    Gossamer::position_type s(idx.select(r));
                    s -= a.asUInt64();
                    unions[s.asUInt64()] += 1;
                    if (inSample)
                    {
                        ints[s.asUInt64()] += 1;
                    }
                }
            }

            for (uint64_t i = 0; i < numSamples; ++i)
            {
                double sim(1.0);
                if (i != sIx)
                {
                    sim = double(ints[i]) / double(unions[sIx]); // double(unions[i]);
                }
                if (ints[i] != 0)
                {
                    cout << names[i] << '\t' << sim << '\n';
                }
            }

            return;
        }

        EspressoCmdSimilarity(const string& pKmersName, const string& pSampleName, uint64_t pNumThreads)
            : mKmersName(pKmersName), mSampleName(pSampleName), mNumThreads(pNumThreads)
        {
        }

    private:
        const string mKmersName;
        const string mSampleName;
        const uint64_t mNumThreads;
    };

    class EspressoCmdFactorySimilarity : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            string g;
            chk.getRepeatingOnce("graph-in", g);

            string s;
            chk.getMandatory("sample", s);

            uint64_t T = 1;
            chk.getOptional("num-threads", T);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new EspressoCmdSimilarity(g, s, T));
        }

        EspressoCmdFactorySimilarity()
            // : GossCmdFactory("calculates the pairwise (Jaccard) similarity of a k-mer set with all others")
            : GossCmdFactory("calculates the proportion of k-mers a set has in common with all others")
        {
            mCommonOptions.insert("graph-in");
            mSpecificOptions.addOpt<string>("sample", "S", "sample name");
        }
    };


} // namespace anonymous

const char*
EspressoApp::name() const
{
    return "espresso";
}

const char*
EspressoApp::version() const
{
    return Gossamer::version;
}

EspressoApp::EspressoApp()
    : App(globalOpts, commonOpts)
{
    cmds.push_back(GossCmdReg("single", GossCmdFactoryPtr(new EspressoCmdFactorySingle)));
    cmds.push_back(GossCmdReg("sparse-single", GossCmdFactoryPtr(new EspressoCmdFactorySparseSingle)));
    cmds.push_back(GossCmdReg("multi", GossCmdFactoryPtr(new EspressoCmdFactoryMulti)));
    cmds.push_back(GossCmdReg("sparse-multi", GossCmdFactoryPtr(new EspressoCmdFactorySparseMulti)));
    cmds.push_back(GossCmdReg("query", GossCmdFactoryPtr(new EspressoCmdFactoryDoQueries)));
    cmds.push_back(GossCmdReg("similarity", GossCmdFactoryPtr(new EspressoCmdFactorySimilarity)));
    cmds.push_back(GossCmdReg("help", GossCmdFactoryPtr(new GossCmdFactoryHelp(*this))));

    globalOpts.addOpt<strings>("debug", "D",
                                "enable particular debugging output");
    globalOpts.addOpt<bool>("help", "h", "show a help message");
    globalOpts.addOpt<string>("log-file", "l", "place to write messages");
    globalOpts.addOpt<strings>("tmp-dir", "", "a directory to use for temporary files (default /tmp)");
    globalOpts.addOpt<uint64_t>("num-threads", "T", "maximum number of worker threads to use, where possible");
    globalOpts.addOpt<bool>("verbose", "v", "show progress messages");
    globalOpts.addOpt<bool>("version", "V", "show the software version");

    commonOpts.addOpt<strings>("graph-in", "G", "graph input");
    commonOpts.addOpt<strings>("fasta-in", "I", "input file in FASTA format");
    commonOpts.addOpt<strings>("fastas-in", "F", "input file containing filenames in FASTA format");
    commonOpts.addOpt<strings>("fastq-in", "i", "input file in FASTQ format");
    commonOpts.addOpt<strings>("fastqs-in", "f", "input file containing filenames in FASTQ format");
    commonOpts.addOpt<strings>("line-in", "", "input file with one sequence per line");
    commonOpts.addOpt<uint64_t>("kmer-size", "K", "kmer size to use (default 25)");
    commonOpts.addOpt<string>("var", "", "variable name to embed in the matrix file");
    commonOpts.addOpt<string>("matrix-out", "o", "matrix file name");
    commonOpts.addOpt<bool>("pairs", "p", "treat reads as paired");
    commonOpts.addOpt<strings>("kmer-set-in", "", "input k-mer set");
    commonOpts.addOpt<strings>("kmer-sets-in", "", "input file containing k-mer sets");
    commonOpts.addOpt<string>("classified-reads-file", "", "file name for reads assigned to genes");
    commonOpts.addOpt<string>("unclassified-reads-file", "", "file name for reads not assigned to genes");
}
