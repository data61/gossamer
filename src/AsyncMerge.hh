#ifndef ASYNCMERGE_HH
#define ASYNCMERGE_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class AsyncMerge
{
public:
    template <typename Kind>
    static void merge(const std::vector<std::string>& pParts, const std::vector<uint64_t>& pSizes,
                      const std::string& pGraphName,
                      uint64_t pK, uint64_t pN, uint64_t pNumThreads, uint64_t pBufferSize,
                      FileFactory& pFactory);
};

#include "AsyncMerge.tcc"

#endif // ASYNCMERGE_HH
