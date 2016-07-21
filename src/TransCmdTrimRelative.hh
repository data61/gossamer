#ifndef TRANSCMDTRIMRELATIVE_HH
#define TRANSCMDTRIMRELATIVE_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class TransCmdFactoryTrimRelative : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    TransCmdFactoryTrimRelative();
};

#endif // TRANSCMDTRIMRELATIVE_HH
