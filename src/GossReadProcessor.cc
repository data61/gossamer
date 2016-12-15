// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossReadProcessor.hh"

#include "LineSource.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossReadSequenceBases.hh"
#include "LineParser.hh"
#include <boost/foreach.hpp>

using namespace std;

typedef vector<string> strings;

namespace {

    void logFile(Logger* pLoggerPtr, const string& pFile)
    {
        if (pLoggerPtr)
        {
            LOG(*pLoggerPtr, info) << "parsing sequences from " << pFile;
        }
    }

    void logRead(UnboundedProgressMonitor* pMonPtr, uint64_t pNumReads)
    {
        if (pMonPtr)
        {
            pMonPtr->tick(pNumReads);
        }
    }

};

void
GossReadProcessor::processSingle(const GossCmdContext& pCxt,
                    const strings& pFastas, const strings& pFastqs, const strings& pLines,
                    GossReadHandler& pHandler, 
                    UnboundedProgressMonitor* pMonPtr, Logger* pLoggerPtr)
{
    FileFactory& fac(pCxt.fac);
    uint64_t numReads = 0;

    LineSourceFactory lineSrcFac(BackgroundLineSource::create);

    BOOST_FOREACH(const std::string& fname, pLines)
    {
        FileThunkIn in(fac, fname);
        logFile(pLoggerPtr, fname);
        pHandler.startFile(fname);
        GossReadParserPtr parser = LineParser::create(lineSrcFac(in));
        for (GossReadSequenceBases seq(parser); seq.valid(); ++seq)
        {
            logRead(pMonPtr, ++numReads);
            pHandler(*seq);
        }
        pHandler.endFile();
    }

    BOOST_FOREACH(const std::string& fname, pFastas)
    {
        FileThunkIn in(fac, fname);
        logFile(pLoggerPtr, fname);
        pHandler.startFile(fname);
        GossReadParserPtr parser = FastaParser::create(lineSrcFac(in));
        for (GossReadSequenceBases seq(parser); seq.valid(); ++seq)
        {
            logRead(pMonPtr, ++numReads);
            pHandler(*seq);
        }
        pHandler.endFile();
    }

    BOOST_FOREACH(const std::string& fname, pFastqs)
    {
        FileThunkIn in(fac, fname);
        logFile(pLoggerPtr, fname);
        pHandler.startFile(fname);
        GossReadParserPtr parser = FastqParser::create(lineSrcFac(in));
        for (GossReadSequenceBases seq(parser); seq.valid(); ++seq)
        {
            logRead(pMonPtr, ++numReads);
            pHandler(*seq);
        }
        pHandler.endFile();
    }

    pHandler.end();
}

void
GossReadProcessor::processPairs(const GossCmdContext& pCxt,
                    const strings& pFastas, const strings& pFastqs, const strings& pLines,
                    GossReadHandler& pHandler,
                    UnboundedProgressMonitor* pMonPtr, Logger* pLoggerPtr)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    uint64_t numPairs = 0;

    LineSourceFactory lineSrcFac(BackgroundLineSource::create);

    for (uint64_t i = 0; i < pLines.size(); i += 2)
    {
        FileThunkIn inLhs(fac, pLines[i]);
        FileThunkIn inRhs(fac, pLines[i+1]);

        GossReadSequenceBases lhs(LineParser::create(lineSrcFac(inLhs)));
        GossReadSequenceBases rhs(LineParser::create(lineSrcFac(inRhs)));

        logFile(pLoggerPtr, inLhs.filename());
        logFile(pLoggerPtr, inRhs.filename());
        while (lhs.valid() && rhs.valid())
        {
            logRead(pMonPtr, ++numPairs);
            pHandler(*lhs, *rhs);
            ++lhs;
            ++rhs;
        }
        if (lhs.valid() != rhs.valid())
        {
            log(warning, "The files '" + pLines[i] + "' and '" + pLines[i + 1] + "' have different numbers of reads!");
        }
    }

    for (uint64_t i = 0; i < pFastas.size(); i += 2)
    {
        FileThunkIn inLhs(fac, pFastas[i]);
        FileThunkIn inRhs(fac, pFastas[i+1]);

        GossReadSequenceBases lhs(FastaParser::create(lineSrcFac(inLhs)));
        GossReadSequenceBases rhs(FastaParser::create(lineSrcFac(inRhs)));

        logFile(pLoggerPtr, inLhs.filename());
        logFile(pLoggerPtr, inRhs.filename());

        while (lhs.valid() && rhs.valid())
        {
            logRead(pMonPtr, ++numPairs);
            pHandler(*lhs, *rhs);
            ++lhs;
            ++rhs;
        }
        if (lhs.valid() != rhs.valid())
        {
            log(warning, "The files '" + pFastas[i] + "' and '" + pFastas[i + 1] + "' have different numbers of reads!");
        }
    }


    for (uint64_t i = 0; i < pFastqs.size(); i += 2)
    {
        FileThunkIn inLhs(fac, pFastqs[i]);
        FileThunkIn inRhs(fac, pFastqs[i+1]);

        GossReadSequenceBases lhs(FastqParser::create(lineSrcFac(inLhs)));
        GossReadSequenceBases rhs(FastqParser::create(lineSrcFac(inRhs)));

        logFile(pLoggerPtr, inLhs.filename());
        logFile(pLoggerPtr, inRhs.filename());

        while (lhs.valid() && rhs.valid())
        {
            logRead(pMonPtr, ++numPairs);
            pHandler(*lhs, *rhs);
            ++lhs;
            ++rhs;
        }
        if (lhs.valid() != rhs.valid())
        {
            log(warning, "The files '" + pFastqs[i] + "' and '" + pFastqs[i + 1] + "' have different numbers of reads!");
        }
    }
    pHandler.end();
}
