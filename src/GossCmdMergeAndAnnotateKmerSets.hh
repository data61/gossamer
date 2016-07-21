#ifndef GOSSCMDMERGEANDANNOTATEKMERSETS_HH
#define GOSSCMDMERGEANDANNOTATEKMERSETS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdMergeAndAnnotateKmerSets : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdMergeAndAnnotateKmerSets(const std::string& pLhs, const std::string& pRhs, const std::string& pOut)
        : mLhs(pLhs), mRhs(pRhs), mOut(pOut)
    {
    }

private:
    const std::string mLhs;
    const std::string mRhs;
    const std::string mOut;
};


class GossCmdFactoryMergeAndAnnotateKmerSets : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryMergeAndAnnotateKmerSets();
};

#endif // GOSSCMDMERGEANDANNOTATEKMERSETS_HH
