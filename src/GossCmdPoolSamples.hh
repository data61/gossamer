#ifndef GOSSCMDPOOLSAMPLES_HH
#define GOSSCMDPOOLSAMPLES_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdPoolSamples : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdPoolSamples(const uint64_t& pK, const uint64_t& pS, 
                       const uint64_t& pN, const uint64_t& pT, 
                       const std::string& pOutPrefix, const strings& pFastaNames, 
                       const strings& pFastqNames, const strings& pKmerSets)
        : mK(pK), mS(pS), mN(pN), mT(pT), mOutPrefix(pOutPrefix),
          mFastaNames(pFastaNames), mFastqNames(pFastqNames),
          mKmerSets(pKmerSets)
    {
    }

private:
    const uint64_t mK;
    const uint64_t mS;
    const uint64_t mN;
    const uint64_t mT;
    const std::string mOutPrefix;
    const strings mFastaNames;
    const strings mFastqNames;
    const strings mKmerSets;
};


class GossCmdFactoryPoolSamples : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryPoolSamples();
};

#endif // GOSSCMDPOOLSAMPLES_HH
