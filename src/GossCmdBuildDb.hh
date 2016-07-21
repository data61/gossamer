#ifndef GOSSCMDBUILDDB_HH
#define GOSSCMDBUILDDB_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

#ifndef PAIRLINKER_HH
#include "PairLinker.hh"
#endif

namespace Sql {
    class Database;
}

class GossCmdBuildDb : public GossCmd
{
public:

    typedef PairLinker::Orientation Orientation;

    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildDb(const std::string& pIn, const std::string& pDb, 
		   bool pResetDb, bool pIncludeGraphLinks,
                   const strings& pFastas, const strings& pFastqs, const strings& pLines,
                   bool pInferCoverage, uint64_t pExpectedCoverage,
                   uint64_t pExpectedInsertSize, double pInsertStdDevFactor, double pInsertTolerance,
                   Orientation pOrientation, uint64_t pNumThreads, 
                   uint64_t pCacheRate, bool pEstimateOnly)
        : mIn(pIn), mDb(pDb), mResetDb(pResetDb), mIncludeGraphLinks(pIncludeGraphLinks),
          mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
          mInferCoverage(pInferCoverage), mExpectedCoverage(pExpectedCoverage),
          mExpectedInsertSize(pExpectedInsertSize), mInsertStdDevFactor(pInsertStdDevFactor),
          mInsertTolerance(pInsertTolerance), mOrientation(pOrientation), mNumThreads(pNumThreads),
          mCacheRate(pCacheRate), mEstimateOnly(pEstimateOnly)
    {
    }

private:
 
    void storeContigs(const GossCmdContext& pCxt, const Graph& pG, const SuperGraph& pSg, Sql::Database& pDb);
    void storeReadLinks(const GossCmdContext& pCxt, const Graph& pG, const SuperGraph& pSg, Sql::Database& pDb, bool pFresh);
    void storeGraphLinks(const GossCmdContext& pCxt, const Graph& pG, const SuperGraph& pSg, Sql::Database& pDb);

    const std::string mIn;
    const std::string mDb;
    const bool mResetDb;
    const bool mIncludeGraphLinks;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const bool mInferCoverage;
    const uint64_t mExpectedCoverage;
    const uint64_t mExpectedInsertSize;
    const double mInsertStdDevFactor;
    const double mInsertTolerance;
    const Orientation mOrientation;
    const uint64_t mNumThreads;
    const uint64_t mCacheRate;
    const bool mEstimateOnly;
};

class GossCmdFactoryBuildDb : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildDb();
};

#endif // GOSSCMDBUILDDB_HH
