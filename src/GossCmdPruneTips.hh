#ifndef GOSSCMDPRUNETIPS_HH
#define GOSSCMDPRUNETIPS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdPruneTips : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdPruneTips(const std::string& pIn, const std::string& pOut,
                     boost::optional<uint64_t> pCutoff,
                     boost::optional<double> pRelCutoff,
                     const uint64_t& pThreads, const uint64_t& pIterations)
        : mIn(pIn), mOut(pOut), mCutoff(pCutoff), mRelCutoff(pRelCutoff),
          mThreads(pThreads), mIterations(pIterations)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const boost::optional<uint64_t> mCutoff;
    const boost::optional<double> mRelCutoff;
    const uint64_t mThreads;
    const uint64_t mIterations;
};
class GossCmdFactoryPruneTips : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryPruneTips();
};

#endif // GOSSCMDPRUNETIPS_HH
