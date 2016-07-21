#ifndef GOSSCMDINTERSECTKMERSETS_HH
#define GOSSCMDINTERSECTKMERSETS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdIntersectKmerSets : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdIntersectKmerSets(const strings& pIns, const std::string& pOut)
        : mIns(pIns), mOut(pOut)
    {
    }

private:
    const strings mIns;
    const std::string mOut;
};


class GossCmdFactoryIntersectKmerSets : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryIntersectKmerSets();
};

#endif // GOSSCMDINTERSECTKMERSETS_HH
