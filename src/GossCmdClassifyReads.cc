// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdClassifyReads.hh"

#include "BackgroundMultiConsumer.hh"
#include "LineSource.hh"
#include "Debug.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "KmerSet.hh"
#include "LineParser.hh"
#include "Phylogeny.hh"
#include "ProgressMonitor.hh"
#include "ReadSequenceFileSequence.hh"
#include "ReadPairSequenceFileSequence.hh"
#include "Spinlock.hh"
#include "SimpleHashSet.hh"

#include <iostream>
#include <map>
#include <algorithm>

using namespace boost;
using namespace std;

typedef vector<string> strings;
typedef Gossamer::position_type Kmer;
typedef vector<Kmer> Kmers;

namespace // anonymous
{
    Debug logAll("log-all-species", "print scoring information for all species");
    Debug printMultiple("print-multiple-hits", "print scored results for multiple species.");
    Debug showKmerHeights("show-kmer-heights", "print a histogram showing how many kmers are at each level in the tree.");

    static const double logE10 = 0.43429448190325176;
    static const uint64_t cacheSize = 65536;
    double* log_facCache;

    void
    prime_cache(void)
    {
        if (log_facCache)
        {
            return;
        }

        log_facCache = (double*)malloc(cacheSize * sizeof(double));
        log_facCache[0] = 0.0;
        // cerr << "priming...." << endl;
        uint64_t f = 1;
        uint64_t n;
        for (n = 1; n < 10; ++n)
        {
            f *= n;
            log_facCache[n] = log((double)f);
        }
        for (n = 10; n < cacheSize; ++n)
        {
            double lf = n * log((double)n) - n + log((double)n * (1 + 4 * n * (1 + 2 * n))) / 6 + log(boost::math::constants::pi<double>()) / 2;
            log_facCache[n] = lf;
        }
        // cerr << "done." << endl;
    }

    static
    double
    log_fac_inner(uint64_t n)
    {
        if (n < cacheSize)
        {
            return log_facCache[n];
        }
        // cerr << n << endl;
        // Ramanujan's approximation.
        return n * log((double)n) - n + log((double)n * (1 + 4 * n * (1 + 2 * n))) / 6 + log(boost::math::constants::pi<double>()) / 2;
    }


    static
    double
    log_fac(uint64_t n)
    {
        return logE10 * log_fac_inner(n);
    }


    static
    double
    log_choose_inner(uint64_t n, uint64_t k)
    {
        return log_fac_inner(n) - log_fac_inner(k) - log_fac_inner(n - k);
    }


    static
    double
    log_choose(uint64_t n, uint64_t k)
    {
        return logE10 * log_choose_inner(n, k);
    }


    static
    double
    log_binom_eq_inner(double logP, double log1mP, uint64_t n, uint64_t k)
    {
        return log_choose_inner(n, k) + k * logP + (n - k) * log1mP;
    }


    static
    double
    log_binom_eq(double p, uint64_t n, uint64_t k)
    {
        return logE10 * log_binom_eq_inner(log(p), log(1.0 - p), n, k);
    }


    static
    double
    log_binom_le(double p, uint64_t n, uint64_t k)
    {
        double logP = log(p);
        double log1mP = log(1.0 - p);
        double r = log_binom_eq_inner(logP, log1mP, n, k);
        int64_t i;
        for (i = k - 1; i >= 0; --i)
        {
            double r0 = r;
            double s = log_binom_eq_inner(logP, log1mP, n, i);
            /* fprintf(stderr, "%f\t%f\n", r, s); */
            int64_t w = r;
            double y1 = r - w;
            double y2 = s - w;
            double z = exp(y1) + exp(y2);
            r = w + log(z);
            if (r == r0)
            {
                break;
            }
        }
        return logE10 * r;
    }


    static
    double
    logBinGE(double logP, uint64_t n, uint64_t k)
    {
        double p = exp(logP);
        double log1mP = log(1.0 - p);
        double r = log_binom_eq_inner(logP, log1mP, n, k);
        uint64_t i;
        for (i = k + 1; i <= n; ++i)
        {
            double r0 = r;
            double s = log_binom_eq_inner(logP, log1mP, n, i);
            int64_t w = r;
            double y1 = r - w;
            double y2 = s - w;
            double z = exp(y1) + exp(y2);
            r = w + log(z);
            if (r == r0)
            {
                break;
            }
        }
        return logE10 * r;
    }

    static
    double
    logPoissonE(double pLam, uint64_t pK)
    {
        return pK * pLam - pLam - log_fac_inner(pK);
    }

    static
    double
    logPoissonLT(double pLam, uint64_t pK)
    {
        double r = logPoissonE(pLam, 0);
        for (uint64_t i = 1; i < pK; ++i)
        {
            double r0 = r;
            double s = logPoissonE(pLam, i);
            int64_t w = r;
            double y1 = r - w;
            double y2 = s - w;
            double z = exp(y1) + exp(y2);
            r = w + log(z);
            if (r == r0)
            {
                break;
            }
        }
        return logE10 * r;
    }

    static
    double
    logPoissonLE(double pLam, uint64_t pK)
    {
        double r = logPoissonE(pLam, 0);
        for (uint64_t i = 1; i <= pK; ++i)
        {
            double r0 = r;
            double s = logPoissonE(pLam, i);
            int64_t w = r;
            double y1 = r - w;
            double y2 = s - w;
            double z = exp(y1) + exp(y2);
            r = w + log(z);
            if (r == r0)
            {
                break;
            }
        }
        return logE10 * r;
    }

    static
    double
    logPoissonGE(double pLam, uint64_t pK)
    {
        if (pK == 0)
        {
            return 0;
        }
        return log10(1 - pow(10, logPoissonLT(pLam, pK)));
    }

    bool subset(const set<uint32_t>& pSub, const set<uint32_t>& pSup)
    {
        for (set<uint32_t>::const_iterator i = pSub.begin(); i != pSub.end(); ++i)
        {
            if (!pSup.count(*i))
            {
                return false;
            }
        }
        return true;
    }

    class EntailmentComparitor
    {
    public:
        bool operator()(const uint64_t& pLhs, const uint64_t& pRhs) const
        {
            const vector<uint32_t>& l(mAncVecs[pLhs]);
            const vector<uint32_t>& r(mAncVecs[pRhs]);
            vector<uint32_t>::const_reverse_iterator lItr = l.rbegin();
            vector<uint32_t>::const_reverse_iterator rItr = r.rbegin();
            while (lItr != l.rend() && rItr != r.rend())
            {
                if (*lItr != *rItr)
                {
                    return *lItr < *rItr;
                }
                ++lItr;
                ++rItr;
            }
            return lItr == l.rend();
        }

        EntailmentComparitor(const vector<vector<uint32_t> >& pAncVecs)
            : mAncVecs(pAncVecs)
        {
        }

    private:
        const vector<vector<uint32_t> >& mAncVecs;
    };

    bool is_entailed_by(const vector<uint32_t>& pLhs, const vector<uint32_t>& pRhs)
    {
        vector<uint32_t>::const_reverse_iterator lItr = pLhs.rbegin();
        vector<uint32_t>::const_reverse_iterator rItr = pRhs.rbegin();
        while (lItr != pLhs.rend() && rItr != pRhs.rend())
        {
            if (*lItr != *rItr)
            {
                return false;
            }
            ++lItr;
            ++rItr;
        }
        return lItr == pLhs.rend();
    }

    class ReadAligner
    {
    public:
        void push_back(const Kmers& pReadStuff)
        {
            uint64_t K(mKmerSet.K());
            vector<uint32_t> cn;
            cn.reserve(pReadStuff.size());

            for (uint64_t i = 0; i < pReadStuff.size(); ++i)
            {
                Kmer n(pReadStuff[i]);
                n.normalize(K);
                KmerSet::Edge e(n);
                uint64_t rnk;
                if (mKmerSet.accessAndRank(e, rnk))
                {
                    cn.push_back(mAnnotations[rnk]);
                }
            }


            if (cn.size() == 0)
            {
                //cerr << pReadStuff.size() << '\t' << cn.size() << endl;
                return;
            }

            Gossamer::sortAndUnique(cn);

            vector<uint64_t> rs;
            vector<vector<uint32_t> > xs(cn.size());
            for (uint64_t i = 0; i < cn.size(); ++i)
            {
                rs.push_back(i);
                mPhylo.ancestors(cn[i], xs[i]);
            }
            EntailmentComparitor entCmp(xs);
            sort(rs.begin(), rs.end(), entCmp);

            vector<uint32_t> ss;
            for (uint64_t i = 1; i < rs.size(); ++i)
            {
                if (!is_entailed_by(xs[rs[i - 1]], xs[rs[i]]))
                {
                    ss.push_back(xs[rs[i - 1]][0]);
                    //const vector<uint32_t>& v(xs[rs[i - 1]]);
                    //for (vector<uint32_t>::const_reverse_iterator j = v.rbegin(); j != v.rend(); ++j)
                    //{
                    //    cerr << '\t' << *j;
                    //}
                    //cerr << endl;
                }
            }
            ss.push_back(xs[rs.back()][0]);
            //const vector<uint32_t>& v(xs[rs.back()]);
            //for (vector<uint32_t>::const_reverse_iterator j = v.rbegin(); j != v.rend(); ++j)
            //{
            //    cerr << '\t' << *j;
            //}
            //cerr << endl;

            //cerr << pReadStuff.size() << '\t' << cn.size() << '\t' << ss.size() << endl;

            uint32_t n = ss[0];

            for (uint64_t i = 1; i < ss.size(); ++i)
            {
                n = mPhylo.lca(n, ss[i]);
                // Multiple spines
                return;
            }

            // Exactly one candidate organism
            SpinlockHolder lk(mResultsMutex);
            mResults[n]++;
        }

        ReadAligner(const KmerSet& pKmerSet, const MappedArray<uint32_t>& pAnnotations,
                    Spinlock& pResultsMutex,
                    map<uint32_t,uint64_t>& pResults,
                    const Phylogeny& pPhylo)
            : mKmerSet(pKmerSet), mAnnotations(pAnnotations),
              mResultsMutex(pResultsMutex), mResults(pResults), mPhylo(pPhylo)
        {
        }

    private:
        const KmerSet& mKmerSet;
        const MappedArray<uint32_t>& mAnnotations;
        Spinlock& mResultsMutex;
        map<uint32_t,uint64_t>& mResults;
        const Phylogeny& mPhylo;
    };
    typedef std::shared_ptr<ReadAligner> ReadAlignerPtr;

    uint64_t counts(Phylogeny& pPhylo, uint32_t pNode, const map<uint32_t,uint64_t>& pCounts)
    {
        AnnotTree::NodePtr n = pPhylo.getNode(pNode);
        uint64_t c = 0;
        map<uint32_t,uint64_t>::const_iterator j = pCounts.find(pNode);
        if (j != pCounts.end())
        {
            c = j->second;
        }
        n->anns["count"] = lexical_cast<string>(c);
        const vector<uint32_t>& kids = pPhylo.kids(pNode);
        for (vector<uint32_t>::const_iterator i = kids.begin(); i != kids.end(); ++i)
        {
            c += counts(pPhylo, *i, pCounts);
        }
        n->anns["sum"] = lexical_cast<string>(c);
        if (c > 0)
        {
            cout << c << '\t' << n->anns.find("kind")->second << '\t' << n->anns.find("name")->second << endl;
        }
        return c;
    }

void extractKmers(uint64_t pK, const GossRead& pRead, Kmers& pKmers)
{
    for (GossRead::Iterator i(pRead, pK); i.valid(); ++i)
    {
        pKmers.push_back(i.kmer());
    }
}

} // namespace anonymous

void
GossCmdClassifyReads::operator()(const GossCmdContext& pCxt)
{
    prime_cache();

    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);;

    KmerSet ref(mIn, fac);

    Phylogeny phylo;
    phylo.read(mIn + ".taxo", fac);

    MappedArray<uint32_t> annot(mIn + ".annotation", fac);

    Spinlock resultsMutex;
    map<uint32_t,uint64_t> results;

    vector<ReadAlignerPtr> aligners;
    BackgroundMultiConsumer<Kmers> grp(128);
    for (uint64_t i = 0; i < mNumThreads; ++i)
    {
        aligners.push_back(ReadAlignerPtr(new ReadAligner(ref, annot, resultsMutex, results, phylo)));
        grp.add(*aligners.back());
    }

    deque<GossReadSequence::Item> items;

    {
        GossReadSequenceFactoryPtr seqFac
            = std::make_shared<GossReadSequenceBasesFactory>();

        GossReadParserFactory lineParserFac(LineParser::create);
        for (auto& f: mLines)
        {
            items.push_back(GossReadSequence::Item(f, lineParserFac, seqFac));
        }

        GossReadParserFactory fastaParserFac(FastaParser::create);
        for (auto& f: mFastas)
        {
            items.push_back(GossReadSequence::Item(f, fastaParserFac, seqFac));
        }

        GossReadParserFactory fastqParserFac(FastqParser::create);
        for (auto& f: mFastqs)
        {
            items.push_back(GossReadSequence::Item(f, fastqParserFac, seqFac));
        }
    }

    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    UnboundedProgressMonitor umon(log, 100000, " reads");

    uint64_t k = ref.K();
    log(info, "Classifying reads....");
    Kmers kmers;
    if (mPairs)
    {
        ReadPairSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);
        for (; reads.valid(); ++reads)
        {
            kmers.clear();
            extractKmers(k, reads.lhs(), kmers);
            extractKmers(k, reads.rhs(), kmers);
            grp.push_back(kmers);
        }
    }
    else
    {
        ReadSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);
        for (; reads.valid(); ++reads)
        {
            kmers.clear();
            extractKmers(k, *reads, kmers);
            grp.push_back(kmers);
        }
    }
    grp.wait();

    counts(phylo, phylo.root(), results);
    //phylo.write(cout);
}

GossCmdPtr
GossCmdFactoryClassifyReads::create(App& pApp, const boost::program_options::variables_map& pOpts)
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

    uint64_t b = 2;
    chk.getOptional("buffer-size", b);
    b *= 1024ULL * 1024ULL * 1024ULL;

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    bool p = false;
    chk.getOptional("pairs", p);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdClassifyReads(in, fastas, fastqs, lines, b, T, p));
}

GossCmdFactoryClassifyReads::GossCmdFactoryClassifyReads()
    : GossCmdFactory("thread the reads through the graph")
{
    mCommonOptions.insert("buffer-size");
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mSpecificOptions.addOpt<bool>("pairs", "",
            "treat reads as pairs");
}
