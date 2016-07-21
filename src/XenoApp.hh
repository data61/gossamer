#ifndef XENOAPP_HH
#define XENOAPP_HH

#ifndef APP_HH
#include "App.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

class XenoApp : public App
{
public:

    const char* name() const;

    const char* version() const;

    XenoApp();
};

#endif // XENOAPP_HH
