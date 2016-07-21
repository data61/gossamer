#ifndef TRANSCMDMERGEGRAPHWITHREFERENCE_HH
#define TRANSCMDMERGEGRAPHWITHREFERENCE_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class TransCmdFactoryMergeGraphWithReference : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    TransCmdFactoryMergeGraphWithReference();
};

#endif // TRANSCMDMERGEGRAPHWITHREFERENCE_HH
