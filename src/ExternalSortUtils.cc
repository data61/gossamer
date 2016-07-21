#include "ExternalSortUtils.hh"
#include <boost/lexical_cast.hpp>

void
ExternalSortBase::genFileName(const std::string& pBase,
        uint64_t pNum, std::string& pName)
{
    pName = pBase;
    pName += boost::lexical_cast<std::string>(pNum);
    pName += ".sort-run";
}

void
ExternalSortBase::genFileName(
        const std::string& pDir,
        const std::string& pBase,
        uint64_t pNum, std::string& pName)
{
    pName = pDir;
    pName += "/";
    pName += pBase;
    pName += boost::lexical_cast<std::string>(pNum);
    pName += ".sort-run";
}


