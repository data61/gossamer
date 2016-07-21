#ifndef GOSSCMDHELP_HH
#define GOSSCMDHELP_HH

#ifndef APP_HH
#include "App.hh"
#endif

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdFactoryHelp : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp,
                      const boost::program_options::variables_map& pOpts);

    GossCmdFactoryHelp(const App& pApp);
};

#endif // GOSSCMDHELP_HH
