// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossApp.hh"

#ifdef KILL_SIGNAL_SUPPORT
#include "GossKillSignal.hh"
#endif

int
main(int argc, char* argv[])
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
