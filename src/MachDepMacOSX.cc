// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Utils.hh"
#include <unistd.h>
#include <execinfo.h>
#include <bitset>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/types.h>

namespace Gossamer { 

namespace MacOSX {

    void printBacktrace()
    {
        void* buffer[50];
        int sz = backtrace(buffer, sizeof(buffer) / sizeof(void*));
        for (int i = 0; i < sz; ++i)
        {
            std::cerr << buffer[i] << ' ';
        }
        std::cerr << '\n';
    }

    void printBacktraceAndExit(int pSig, siginfo_t* pInfo, void* pUcontext_t)
    {
        std::cerr << "Caught signal " << pSig << " (" << strsignal(pSig) << ")\n";
        printBacktrace();
        exit(1);
    }

    void printBacktraceAndContinue(int pSig, siginfo_t* pInfo, void* pUcontext_t)
    {
        std::cerr << "Caught signal " << pSig << " (" << strsignal(pSig) << ")\n"; 
        printBacktrace();
    }

    void
    installSignalHandlers()
    {
        struct sigaction sigact = {};
        sigact.sa_sigaction = &printBacktraceAndExit;
        sigact.sa_flags = SA_RESTART | SA_SIGINFO;

        int sigs[] = {SIGILL, SIGFPE, SIGSEGV, SIGBUS};
        for (unsigned i = 0; i < sizeof(sigs) / sizeof(int); ++i)
        {
            const int sig = sigs[i];
            if (sigaction(sig, &sigact, 0))
            {
                std::cerr << "Error setting signal handler for " << sig << " (" << strsignal(sig) << ")\n";
                exit(1);
            } 
        }

        sigact.sa_sigaction = &printBacktraceAndContinue;
        const int sig = SIGUSR1;
        if (sigaction(sig, &sigact, 0))
        {
            std::cerr << "Error setting signal handler for " << sig << " (" << strsignal(sig) << ")\n";
            exit(1);
        } 
    }

    enum {
        kCpuCapPopcnt = 0,
        kCpuCapLast
    };

    std::bitset<kCpuCapLast> sCpuCaps;

    uint32_t sLogicalProcessorCount;

    void probeCpu()
    {
        sLogicalProcessorCount = 1;
        sCpuCaps[kCpuCapPopcnt] = false;

        uint32_t logicalcpu;
        size_t logicalcpuLen = sizeof(logicalcpu);
        if (!sysctlbyname("hw.logicalcpu", &logicalcpu, &logicalcpuLen, NULL, 0)) {
            sLogicalProcessorCount = logicalcpu;
        }

        uint32_t features[2];
        size_t featuresLen = sizeof(features);
        if (!sysctlbyname("machdep.cpu.feature_bits", &features, &featuresLen, NULL, 0)) {
            sCpuCaps[kCpuCapPopcnt] = features[1] & (1 << 23);
        }
    }

}   // namespace MacOSX

    std::string defaultTmpDir()
    {
        const char* fallback = "/tmp";
        const char* tmpdir = getenv( "TMPDIR" );
        return tmpdir ? tmpdir : fallback;
    }

    uint32_t
    logicalProcessorCount()
    {
        return MacOSX::sLogicalProcessorCount;
    }
}

void
MachineAutoSetup::setupMachineSpecific()
{
    Gossamer::MacOSX::probeCpu();
    Gossamer::MacOSX::installSignalHandlers();
}
