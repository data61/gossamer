#ifndef GOSSAPP_HH
#define GOSSAPP_HH

#ifndef APP_HH
#include "App.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

class GossApp : public App
{
public:

    const char* name() const;

    const char* version() const;

    GossApp();
};

#endif // GOSSAPP_HH
