// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSKILLSIGNAL_HH
#define GOSSKILLSIGNAL_HH

#include <string>
#include <iostream>
#include <thread>
#include <boost/filesystem.hpp>

static const std::string KILL_SIGNAL_CMD_OPTION = "--kill-signal";

namespace {

//bool parseAndRegisterKillSignal(int& argc, char* argv[], bool bRemoveFromArgs = false, int pCheckEveryMsec = 1000);
//void registerKillSignal(const std::string& pSignalFileName, int pCheckEveryMsec = 1000 );

class GossKillSignal
{
public:
    static bool ParseAndRegister(int& argc, char* argv[], bool bRemoveFromArgs = false, int pCheckEveryMsec = 1000);
    static void Register(const std::string& pSignalFileName, int pCheckEveryMsec = 1000 );
    static void JoinThread();

    void operator()();
    ~GossKillSignal();

private:
    GossKillSignal(const std::string& pSignalFileName, int pCheckEveryMsec);
    void step();
    
    std::string mSignalFile; // if file exists, kill
    int mCheckEveryMsec; // check if file exists every msec   
    bool mStop;    
    std::shared_ptr<std::thread> mThread;

    static std::shared_ptr<GossKillSignal> mInstance;
};

std::shared_ptr<GossKillSignal> GossKillSignal::mInstance;

using namespace std;
using namespace boost;

GossKillSignal::GossKillSignal(const string& pSignalFile, int pCheckEveryMsec)
    : mSignalFile( pSignalFile )
    , mCheckEveryMsec( pCheckEveryMsec )
    , mStop( false )
{
    // nothing
}


GossKillSignal::~GossKillSignal()
{
    // nothing yet
}

void GossKillSignal ::operator()()
{
    while( !mStop )
    {
        step();
    }
    cerr << "GossKillSignal thread finished" << endl;
}

void GossKillSignal::step()
{
    this_thread::sleep( posix_time::milliseconds( mCheckEveryMsec ) );
    //fprintf( stderr, "Checking for kill signal file" );

    if( filesystem::exists(mSignalFile) )
    {
        exit(1);
    }
}

void GossKillSignal::JoinThread()
{
    if( mInstance != 0 )
    {
        mInstance->mStop = true;
        mInstance->mThread->join();
    }
}

void GossKillSignal::Register(const std::string& pSignalFileName, int pCheckEveryMsec )
{
    cerr << "Here" << endl;
    mInstance = std::make_shared<GossKillSignal>(pSignalFileName, pCheckEveryMsec);
    mInstance->mThread = std::make_shared<thread>( boost::ref(*GossKillSignal::mInstance) ); // start the thread that checks for signals
}

bool GossKillSignal::ParseAndRegister(int& argc, char* argv[], bool bRemoveFromArgs, int pCheckEveryMsec)
{
    for( int i = 1; i < argc; ++i )
    {
        if( KILL_SIGNAL_CMD_OPTION.compare( argv[i] ) == 0) 
        {
            if( i + 1 >= argc )
            {
                cerr << "Invalid argument for: " << KILL_SIGNAL_CMD_OPTION << endl;
                return false;
            }

            // start the monitoring thread
            Register( argv[i+1], pCheckEveryMsec );

            if( bRemoveFromArgs ) // remove the killsignal option from the arg list
            {
                for( i += 2; i < argc; ++i )
                {
                    argv[i-2] = argv[i];
                }
                argc = i-2;
                argv[argc] = 0; // needs to be null terminated
            }

            break;
        }
    }

    return true;
}

}

#endif // #ifndef GOSSKILLSIGNAL_HH
