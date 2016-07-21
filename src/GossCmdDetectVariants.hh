#ifndef GOSSCMDDETECTVARIANTS_HH
#define GOSSCMDDETECTVARIANTS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdDetectVariants : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdDetectVariants(const std::string& pRef, const std::string& pTarget)
        : mRef(pRef), mTarget(pTarget)
    {
    }

private:
    const std::string mRef;
    const std::string mTarget;
};


class GossCmdFactoryDetectVariants : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryDetectVariants();
};

#endif // GOSSCMDDETECTVARIANTS_HH
