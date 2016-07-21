#ifndef GOSSCMDBUILDENTRYEDGESET_HH
#define GOSSCMDBUILDENTRYEDGESET_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildEntryEdgeSet : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildEntryEdgeSet(const std::string& pIn, const uint64_t& pThreads)
        : mIn(pIn), mThreads(pThreads)
    {
    }

private:
    const std::string mIn;
    const uint64_t mThreads;
};

class GossCmdFactoryBuildEntryEdgeSet : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildEntryEdgeSet();
};

#endif // GOSSCMDBUILDENTRYEDGESET_HH
