#ifndef GOSSCMDBUILDEDGEINDEX_HH
#define GOSSCMDBUILDEDGEINDEX_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildEdgeIndex : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildEdgeIndex(const std::string& pIn, uint64_t pNumThreads, uint64_t pCacheRate)
        : mIn(pIn), mNumThreads(pNumThreads), mCacheRate(pCacheRate)
    {
    }

private:
    const std::string mIn;
    const uint64_t mNumThreads;
    const uint64_t mCacheRate;
};


class GossCmdFactoryBuildEdgeIndex : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildEdgeIndex();
};

#endif // GOSSCMDBUILDEDGEINDEX_HH
