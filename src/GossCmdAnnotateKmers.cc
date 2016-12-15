// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdAnnotateKmers.hh"

#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "FastaParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "KmerSet.hh"
#include "Phylogeny.hh"
#include "RunLengthCodedSet.hh"
#include "Spinlock.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace // anonymous
{

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
        log_facCache[n] = log((long double)f);
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
double log_fac(uint64_t n)
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
        double s = log_binom_eq_inner(logP, log1mP, n, i);
        /* fprintf(stderr, "%f\t%f\n", r, s); */
        int64_t w = r;
        double y1 = r - w;
        double y2 = s - w;
        double z = exp(y1) + exp(y2);
        r = w + log(z);
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

class CounterAnnot
{
public:
    void operator()(ostream& pOut, uint64_t pNode) const
    {
        map<uint64_t,uint64_t>::const_iterator i = mCounts.find(pNode);
        if (i == mCounts.end())
        {
            pOut << 0;
        }
        else
        {
            pOut << i->second;
        }
        i = mOrgCounts.find(pNode);
        if (i != mOrgCounts.end())
        {
            pOut << '/' << i->second;
        }
    }

    CounterAnnot(const map<uint64_t,uint64_t>& pCounts, const map<uint64_t,uint64_t>& pOrgCounts)
        : mCounts(pCounts), mOrgCounts(pOrgCounts)
    {
    }

private:
    const map<uint64_t,uint64_t>& mCounts;
    const map<uint64_t,uint64_t>& mOrgCounts;
};

class KmerClasses
{
    static const uint32_t uninitd;
    static const uint64_t L;

public:

    void set(uint64_t pRank, uint32_t pVal)
    {
        SpinlockHolder lck(mLocks[pRank % L]);
        uint32_t& c(mClasses[pRank]);
        if (c == uninitd)
        {
            c = pVal;
        }
        else
        {
            c = mPhylo.lca(c, pVal);
        }
    }
 
    uint64_t size() const
    {
        return mClasses.size();
    }

    uint32_t operator[](uint64_t pIx) const
    {
        return mClasses[pIx];
    }

    KmerClasses(const Phylogeny& pPhylo, uint64_t pCount)
        : mPhylo(pPhylo), mClasses(pCount, uninitd), mLocks(L)
    {
    }

private:

    const Phylogeny& mPhylo;
    vector<uint32_t> mClasses;
    vector<Spinlock> mLocks;
};

const uint32_t KmerClasses::uninitd = -1;
const uint64_t KmerClasses::L = 1ULL << 16;

class Annotater
{
public:

    struct Item
    {
        Item()
            : mFilename(), mClass(), mReads()
        {
        }

        Item(const string& pFilename, uint32_t pClass, GossReadSequencePtr pReads)
            : mFilename(pFilename), mClass(pClass), mReads(pReads)
        {
        }

        string mFilename;
        uint32_t mClass; 
        GossReadSequencePtr mReads;
    };

    void push_back(const Item pItem)
    {
        mLog(info, "Computing kmers for " + pItem.mFilename);
        for (GossReadSequence& rs = *pItem.mReads; rs.valid(); ++rs)
        {
            const GossRead& r = *rs;
            for (GossRead::Iterator i(r, mK); i.valid(); ++i)
            {
                Gossamer::position_type kmer = i.kmer();
                kmer.normalize(mK);
                uint64_t rnk = 0;
                if (mRef.accessAndRank(KmerSet::Edge(kmer), rnk))
                {
                    mClasses.set(rnk, pItem.mClass);
                }
            }
        }
    }

    Annotater(uint64_t pK, const KmerSet& pRef, KmerClasses& pClasses, Logger& pLog)
        : mK(pK), mRef(pRef), mClasses(pClasses), mLog(pLog)
    {
    }

private:

    const uint64_t mK;
    const KmerSet& mRef;
    KmerClasses& mClasses;
    Logger& mLog;
};

typedef std::shared_ptr<Annotater> AnnotaterPtr;

uint64_t annCounts(Phylogeny& pPhylo, const map<uint32_t, uint64_t>& pCounts, uint32_t pNode)
{
    uint64_t t = 0;
    const vector<uint32_t>& kids(pPhylo.kids(pNode));
    for (uint64_t i = 0; i < kids.size(); ++i)
    {
        t += annCounts(pPhylo, pCounts, kids[i]);
    }
    map<uint32_t, uint64_t>::const_iterator itrC(pCounts.find(pNode));
    uint64_t c = itrC != pCounts.end() ? itrC->second : 0;
    t += c;

    AnnotTree::NodePtr& n(pPhylo.getNode(pNode));
    n->anns["count"] = lexical_cast<string>(c);
    n->anns["total"] = lexical_cast<string>(t);
    return t;
}

void annCounts(Phylogeny& pPhylo, const map<uint32_t, uint64_t>& pCounts)
{
    annCounts(pPhylo, pCounts, pPhylo.root());
}

} // namespace anonymous

void
GossCmdAnnotateKmers::operator()(const GossCmdContext& pCxt)
{
    prime_cache();

    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    Timer t;

    KmerSet ref(mRef, fac);

    map<string,uint32_t> ins;
    {
        FileFactory::InHolderPtr inp = fac.in(mAnnotList);
        istream& in(**inp);
        while (in.good())
        {
            string m;
            uint32_t l;
            in >> m >> l;
            if (!in.good())
            {
                break;
            }
            ins[m] = l;
        }
    }

    map<int64_t,uint64_t> idx;
    Phylogeny phylo;
    // phylo.read(mRef + ".taxo", fac);
    phylo.read(mMergeList, fac);

    const uint64_t k = ref.K();

    cerr << "allocating " << ref.count() << " counts." <<endl;
    KmerClasses orgs(phylo, ref.count());

    BackgroundMultiConsumer<Annotater::Item> grp(128);
    vector<AnnotaterPtr> anns;
    for (uint64_t i = 0; i < mNumThreads; ++i)
    {
        anns.push_back(AnnotaterPtr(new Annotater(k, ref, orgs, log)));
        grp.add(*anns.back());
    }

    GossReadParserFactory fastaParserFac(FastaParser::create);
    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    GossReadSequenceBasesFactory seqFac;

    for (map<string,uint32_t>::const_iterator i = ins.begin(); i != ins.end(); ++i)
    {
        FileThunkIn in(fac, i->first);
        GossReadSequencePtr readsPtr(seqFac.create(fastaParserFac(lineSrcFac(in))));
        Annotater::Item itm(i->first, i->second, readsPtr);
        grp.push_back(itm);
    }
    grp.wait();

    log(info, "writing out lca index.");
    MappedArray<uint32_t>::Builder bld(mRef + ".annotation", fac);
    map<uint32_t,uint64_t> counts;
    for (uint64_t i = 0; i < orgs.size(); ++i)
    {
        counts[orgs[i]]++;
        bld.push_back(orgs[i]);
    }
    bld.end();

    log(info, "writing count annotations.");
    annCounts(phylo, counts);
    FileFactory::OutHolderPtr op(fac.out(mMergeList));
    ostream& o(**op);
    phylo.write(o);

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryAnnotateKmers::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string ref;
    chk.getRepeatingOnce("graph-in", ref);

    string annot;
    chk.getMandatory("annotation-list", annot);

    string merge;
    chk.getMandatory("phylogeny", merge);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdAnnotateKmers(ref, annot, merge, T));
}

GossCmdFactoryAnnotateKmers::GossCmdFactoryAnnotateKmers()
    : GossCmdFactory("Decorate a graph with an assignment of kmers to graphs.")
{
    mCommonOptions.insert("graph-in");

    mSpecificOptions.addOpt<string>("annotation-list", "",
            "A file containing a list of graph names, 1 per line.");
    mSpecificOptions.addOpt<string>("phylogeny", "",
            "A file containing a list of merges to perform to construct the phylogenetic tree.");
}
