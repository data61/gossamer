#ifndef ESPRESSOAPP_HH
#define ESPRESSOAPP_HH

#ifndef APP_HH
#include "App.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

class EspressoApp : public App
{
public:

    const char* name() const;

    const char* version() const;

    EspressoApp();
};

#endif // ESPRESSOAPP_HH
