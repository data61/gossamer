// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
//#include "Utils.hh"

#include "Utils.hh"
#include <bitset>
#include <signal.h>
#include <DbgHelp.h>

#pragma intrinsic(__popcnt64)
#pragma intrinsic(__cpuid)
#pragma intrinsic(__cpuidex)

namespace Gossamer {
    
    std::string defaultTmpDir()
    {
        const char* fallBack = "."; // should not happen really, but lets give reasonable default
        const char* val = getenv( "temp" );
        return val ? val : fallBack;
    }

}


namespace Gossamer { namespace Windows {

    const char* sigCodeString(int pSig )
    {
        switch (pSig)
        {
        case SIGINT:   return "interrupt";
        case SIGILL:   return "illegal instruction - invalid function image";
        case SIGFPE:   return "floating point exception";
        case SIGSEGV:  return "segment violation";
        case SIGTERM:  return "Software termination signal from kill";
        case SIGBREAK: return "Ctrl-Break sequence";
        case SIGABRT:  return "abnormal termination triggered by abort call";
        default: break;
        }

        return "unknown signal code";
    }

    void printBacktrace()
    {
         unsigned int   i;
         void         * stack[ 100 ];
         unsigned short frames;
         SYMBOL_INFO  * symbol;
         HANDLE         process;

         process = GetCurrentProcess();

         SymInitialize( process, NULL, TRUE );

         frames               = CaptureStackBackTrace( 0, 100, stack, NULL );
         symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
         symbol->MaxNameLen   = 255;
         symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

         for( i = 0; i < frames; i++ )
         {
         SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );

         printf( "%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address );
         }

         free( symbol );
    }

    void printBacktraceAndExit(int pSig)
    {
        std::cerr << "Caught signal " << pSig << ": " << sigCodeString(pSig) << "\n";
        printBacktrace();
        exit(1);
    }

    void installSignalHandlers()
    {
        int sigs[] = {SIGILL, SIGFPE, SIGSEGV};
        for (unsigned i = 0; i < sizeof(sigs) / sizeof(int); ++i)
        {
            const int sig = sigs[i];
            if ( SIG_ERR == signal(sig, &printBacktraceAndExit) )
            {
                std::cerr << "Error setting signal handler for " << sig << "\n";
                exit(1);
            } 
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

        int cpuInfo[4];
        __cpuid(cpuInfo, 0);

        int cpuInfoExt[4];
        __cpuid(cpuInfoExt, 0x80000000);

        if (cpuInfo[0] >= 1)
        {
            int cpuInfo1[4];
            __cpuid(cpuInfo1, 1);
            sLogicalProcessorCount = (cpuInfo1[1] >> 16) & 0xFF;
            sCpuCaps[kCpuCapPopcnt] = cpuInfo1[2] & (1 << 23);
        }
    }

    static uint8_t sPopcntLut[256];

    uint32_t popcntByLut(uint64_t pValue)
    {
        uint32_t ret = 0;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        ret += sPopcntLut[pValue & 0xff]; pValue >>= 8;
        return ret;
    }

    uint32_t popcntByIntrinsic(uint64_t pValue)
    {
        return (uint32_t)__popcnt64(pValue);
    }

    uint32_t (*sPopcnt64)(uint64_t pWord) = &popcntByLut;

    void setupPopcnt()
    {
        for (uint64_t i = 0; i < 256; ++i)
        {
            uint8_t ii = i;
            uint8_t val = 0;
            val += ii & 1; ii >>= 1;
            val += ii & 1; ii >>= 1;
            val += ii & 1; ii >>= 1;
            val += ii & 1; ii >>= 1;
            val += ii & 1; ii >>= 1;
            val += ii & 1; ii >>= 1;
            val += ii & 1; ii >>= 1;
            val += ii & 1; ii >>= 1;
            sPopcntLut[i] = val;
        }

        sPopcnt64 = sCpuCaps[kCpuCapPopcnt] ? &popcntByIntrinsic : &popcntByLut;
    }

} }


void
MachineAutoSetup::setupMachineSpecific()
{
    Gossamer::Windows::installSignalHandlers();
    Gossamer::Windows::probeCpu();
    Gossamer::Windows::setupPopcnt();
}


uint32_t
Gossamer::logicalProcessorCount()
{
    return Gossamer::Windows::sLogicalProcessorCount;
}


void gettimeofday(timeval* t, void *)
{
    using namespace boost::posix_time;

    static const ptime timeAtEpoch( boost::gregorian::date(1970,1,1) ); 
    
    time_duration diff = microsec_clock::local_time() - timeAtEpoch;
    
    t->tv_sec = diff.total_seconds();
    
    diff -= seconds(t->tv_sec); // get only the fractional second part
    t->tv_usec = diff.total_microseconds();
}


