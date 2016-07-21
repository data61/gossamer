#include "GossApp.hh"

#ifdef KILL_SIGNAL_SUPPORT
#include "GossKillSignal.hh"
#endif

int main(int argc, char* argv[])
{
#ifdef KILL_SIGNAL_SUPPORT
    if( !GossKillSignal::ParseAndRegister(argc, argv, true) )
    {
        exit(1);
    }
#endif

    GossApp app;
    int exitCode = app.main(argc, argv);

#ifdef KILL_SIGNAL_SUPPORT
    GossKillSignal::JoinThread();
#endif
    return exitCode;
}
