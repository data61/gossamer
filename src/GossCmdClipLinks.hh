#ifndef GOSSCMDCLIPLINKS_HH
#define GOSSCMDCLIPLINKS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdClipLinks : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdClipLinks(const std::string& pIn, const std::string& pOut)
        : mIn(pIn), mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
};


class GossCmdFactoryClipLinks : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryClipLinks();
};

#endif // GOSSCMDCLIPLINKS_HH
