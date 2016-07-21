#ifndef TRANSLUCENTAPP_HH
#define TRANSLUCENTAPP_HH

#ifndef APP_HH
#include "App.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

class TranslucentApp : public App
{
public:

    const char* name() const;

    const char* version() const;

    TranslucentApp();
};

#endif // TRANSLUCENTAPP_HH
