#ifndef GOSSCMDSUBTRACTKMERSET_HH
#define GOSSCMDSUBTRACTKMERSET_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdSubtractKmerSet : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdSubtractKmerSet(const strings& pIns, const std::string& pOut)
        : mIns(pIns), mOut(pOut)
    {
    }

private:
    const strings mIns;
    const std::string mOut;
};


class GossCmdFactorySubtractKmerSet : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactorySubtractKmerSet();
};

#endif // GOSSCMDSUBTRACTKMERSET_HH
