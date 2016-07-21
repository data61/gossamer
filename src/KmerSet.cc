#include "FileFactory.hh"
#include "KmerSet.hh"

#include <string>

using namespace std;

void
KmerSet::remove(const string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".header");
    SparseArray::remove(pBaseName + ".kmers", pFactory);
}
