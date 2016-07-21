#ifndef EXTERNALSORT64_HH
#define EXTERNALSORT64_HH

#ifndef EXTERNALSORTUTILS_HH
#include "ExternalSortUtils.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif // LOGGER_HH

template <typename T, typename V = ExternalSortCodec<T> >
class ExternalSort : public ExternalSortBase
{
public:
    template <typename SrcItr, typename DestItr>
    static void sort(SrcItr& pSrc, DestItr& pDest,
                     FileFactory& pFactory, uint64_t pBufSpace,
                     Logger* pLogger);
};


#include "ExternalSort64.tcc"

#endif // EXTERNALSORT64_HH
