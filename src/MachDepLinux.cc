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
#include <time.h>
#include <sys/signal.h>

namespace Gossamer {
    
    std::string defaultTmpDir()
    {
        const char* fallback = "/tmp";
        const char* tmpdir = getenv( "TMPDIR" );
        return tmpdir ? tmpdir : fallback;
    }

}

namespace Gossamer { namespace Linux {

    const char*
    sigCodeString(int pSig, int pCode)
    {
        switch (pCode)
        {
        case SI_USER:    return "kill(2) or raise(3)";
        case SI_KERNEL:  return "Sent by the kernel.";
        case SI_QUEUE:   return "sigqueue(2)";
        case SI_TIMER:   return "POSIX timer expired";
        case SI_MESGQ:   return "POSIX message queue state changed (since Linux 2.6.6); see mq_notify(3)";
        case SI_ASYNCIO: return "AIO completed";
        case SI_SIGIO:   return "queued SIGIO";
        case SI_TKILL:   return "tkill(2) or tgkill(2) (since Linux 2.4.19)";
        }

        switch (pSig)
        {
        case SIGILL:
            switch (pCode)
            {
            case ILL_ILLOPC: return "illegal opcode";
            case ILL_ILLOPN: return "illegal operand";
            case ILL_ILLADR: return "illegal addressing mode";
            case ILL_ILLTRP: return "illegal trap";
            case ILL_PRVOPC: return "privileged opcode";
            case ILL_PRVREG: return "privileged register";
            case ILL_COPROC: return "coprocessor error";
            case ILL_BADSTK: return "internal stack error";
            default: break;
            }
            break;

        case SIGFPE:
            switch (pCode)
            {
            case FPE_INTDIV: return "integer divide by zero";
            case FPE_INTOVF: return "integer overflow";
            case FPE_FLTDIV: return "floating-point divide by zero";
            case FPE_FLTOVF: return "floating-point overflow";
            case FPE_FLTUND: return "floating-point underflow";
            case FPE_FLTRES: return "floating-point inexact result";
            case FPE_FLTINV: return "floating-point invalid operation";
            case FPE_FLTSUB: return "subscript out of range";
            default: break;
            }
            break;

        case SIGSEGV:
            switch (pCode)
            {
               case SEGV_MAPERR: return "address not mapped to object";
               case SEGV_ACCERR: return "invalid permissions for mapped object";
               default: break;
            }
            break;

        case SIGBUS:
            switch (pCode)
            {
               case BUS_ADRALN: return "invalid address alignment";
               case BUS_ADRERR: return "nonexistent physical address";
               case BUS_OBJERR: return "object-specific hardware error ";
               default: break;
            }
            break;

        default:
            break;
        }

        return "unknown signal code";
    }

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
        std::cerr << "Caught signal " << pSig << " (" << strsignal(pSig) << "): " << sigCodeString(pSig, pInfo->si_code) << "\n";
        printBacktrace();
        exit(1);
    }

    void printBacktraceAndContinue(int pSig, siginfo_t* pInfo, void* pUcontext_t)
    {
        std::cerr << "Caught signal " << pSig << " (" << strsignal(pSig) << "): "; 
        printBacktrace();
    }


    void
    installSignalHandlers()
    {
        struct sigaction sigact = {};
        sigact.sa_sigaction = &printBacktraceAndExit;
        sigact.sa_flags = SA_RESTART || SA_SIGINFO;

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

    inline void
    cpuid(uint32_t pInfoType, uint32_t pInfo[4])
    {
        pInfo[0] = pInfoType;
        __asm__ __volatile__(
            // ebx is used for PIC on 32-bit. We officially
            // don't support 32-bit, but it's just as easy to
            // avoid clobbering it.
            "mov %%ebx,%%edi;"
            "cpuid;"
            "mov %%ebx, %%esi;"
            "mov %%edi, %%edx;"
            : "+a" (pInfo[0]),
              "=S" (pInfo[1]),
              "=c" (pInfo[2]),
              "=d" (pInfo[3])
            : : "edi");
    }

    void probeCpu()
    {
        sLogicalProcessorCount = 1;
        sCpuCaps[kCpuCapPopcnt] = false;

        uint32_t cpuInfo[4];
        cpuid(0, cpuInfo);

        uint32_t cpuInfoExt[4];
        cpuid(0x80000000, cpuInfoExt);

        if (cpuInfo[0] >= 1)
        {
            uint32_t cpuInfo1[4];
            cpuid(1, cpuInfo1);
            sLogicalProcessorCount = (cpuInfo1[1] >> 16) & 0xFF;
            sCpuCaps[kCpuCapPopcnt] = cpuInfo1[2] & (1 << 23);
        }
    }
} }


void
MachineAutoSetup::setupMachineSpecific()
{
    Gossamer::Linux::probeCpu();
    Gossamer::Linux::installSignalHandlers();
}


uint32_t
Gossamer::logicalProcessorCount()
{
    return Gossamer::Linux::sLogicalProcessorCount;
}


