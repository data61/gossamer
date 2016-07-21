#ifndef TRANSCMDASSEMBLE_HH
#define TRANSCMDASSEMBLE_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif


class TransCmdFactoryAssemble : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    TransCmdFactoryAssemble();
};

#endif // TRANSCMDASSEMBLE_HH
