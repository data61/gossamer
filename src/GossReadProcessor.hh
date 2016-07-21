#ifndef GOSSREADPROCESSOR_HH
#define GOSSREADPROCESSOR_HH

#ifndef GOSSCMDCONTEXT_HH
#include "GossCmdContext.hh"
#endif

#ifndef GOSSREADHANDLER_HH
#include "GossReadHandler.hh"
#endif

#ifndef UNBOUNDEDPROGRESSMONITOR_HH
#include "UnboundedProgressMonitor.hh"
#endif

class GossReadProcessor
{
public:
    typedef std::vector<std::string> strings;

    static void processSingle(const GossCmdContext& pCxt,
                    const strings& pFastas, const strings& pFastqs, const strings& pLines,
                    GossReadHandler& pHandler,
                    UnboundedProgressMonitor* pMonPtr = 0, Logger* pLoggerPtr=0);

    static void processPairs(const GossCmdContext& pCxt,
                    const strings& pFastas, const strings& pFastqs, const strings& pLines,
                    GossReadHandler& pHandler,
                    UnboundedProgressMonitor* pMonPtr = 0, Logger* pLoggerPtr=0);
};

#endif // GOSSREADPROCESSOR_HH
