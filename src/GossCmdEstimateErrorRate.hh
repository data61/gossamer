
#ifndef GOSSCMDESTIMATEERRORRATE_HH
#define GOSSCMDESTIMATEERRORRATE_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdEstimateErrorRate : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdEstimateErrorRate(const std::string& pRef)
        : mRef(pRef)
    {
    }

private:
    const std::string mRef;
};


class GossCmdFactoryEstimateErrorRate : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryEstimateErrorRate();
};

#endif // GOSSCMDESTIMATEERRORRATE_HH
