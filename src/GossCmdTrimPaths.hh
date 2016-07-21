#ifndef GOSSCMDTRIMPATHS_HH
#define GOSSCMDTRIMPATHS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdTrimPaths : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdTrimPaths(const std::string& pIn, const std::string& pOut, uint32_t pC, uint64_t pThreads)
        : mIn(pIn), mOut(pOut), mC(pC), mThreads(pThreads)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const uint32_t mC;
    const uint64_t mThreads;
};
class GossCmdFactoryTrimPaths : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryTrimPaths();
};

#endif // GOSSCMDTRIMPATHS_HH
