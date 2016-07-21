#ifndef ELECTAPP_HH
#define ELECTAPP_HH

#ifndef APP_HH
#include "App.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

class ElectApp : public App
{
public:

    const char* name() const;

    const char* version() const;

    ElectApp();
};

#endif // ELECTAPP_HH
