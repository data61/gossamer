#ifndef GOSSCMDCOMPUTENEARKMERS_HH
#define GOSSCMDCOMPUTENEARKMERS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdComputeNearKmers : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdComputeNearKmers(const std::string& pIn, uint64_t pNumThreads)
        : mIn(pIn), mNumThreads(pNumThreads)
    {
    }

private:
    const std::string mIn;
    const uint64_t mNumThreads;
};


class GossCmdFactoryComputeNearKmers : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryComputeNearKmers();
};

#endif // GOSSCMDCOMPUTENEARKMERS_HH
