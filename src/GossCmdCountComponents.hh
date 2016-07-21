#ifndef GOSSCMDCOUNTCOMPONENTS_HH
#define GOSSCMDCOUNTCOMPONENTS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdCountComponents : public GossCmd
{
    typedef std::vector<std::string> strings;

public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdCountComponents(const std::string& pIn, const std::string& pOut,
                           const strings& pFastaNames, const strings& pFastqNames, const strings& pLineNames)
        : mIn(pIn), mOut(pOut),
          mFastaNames(pFastaNames), mFastqNames(pFastqNames), mLineNames(pLineNames)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const strings mFastaNames;
    const strings mFastqNames;
    const strings mLineNames;
};


class GossCmdFactoryCountComponents : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryCountComponents();
};

#endif // GOSSCMDCOUNTCOMPONENTS_HH
