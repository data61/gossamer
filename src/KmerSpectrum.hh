#ifndef KMERSPECTRUM_HH
#define KMERSPECTRUM_HH

#include "GossReadHandler.hh"
#include "RankSelect.hh"
#include "KmerSet.hh"

class KmerSpectrum
{
public:
    typedef std::vector<std::string> strings;

    static GossReadHandlerPtr singleRowBuilder(uint64_t pK, const std::string& pVarName,
                                               const std::string& pFileName);

    static GossReadHandlerPtr multiRowBuilder(uint64_t pK, const std::string& pVarName,
                                              const std::string& pFileName);

    static GossReadHandlerPtr sparseSingleRowBuilder(const KmerSet& pKmerSet,
                                              const std::string& pKmersName,
                                              const std::string& pVarName,
                                              const std::string& pFileName);

    static GossReadHandlerPtr sparseMultiRowBuilder(const KmerSet& pKmerSet,
                                              const std::string& pKmersName,
                                              const strings& pInitKmerSets,
                                              bool pPerFile,
                                              FileFactory& pFactory);
};

#endif // KMERSPECTRUM_HH
