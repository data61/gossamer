#include "VariableWidthBitArray.hh"

VariableWidthBitArray::Builder::Builder(const std::string& pBaseName, FileFactory& pFactory)
    : mFile(pBaseName, pFactory),
      mPos(0), mCurrWord(0)
{
}

void
VariableWidthBitArray::Builder::flush()
{
    mFile.push_back(mCurrWord);
}

void
VariableWidthBitArray::Builder::end()
{
    flush();
    mFile.end();
}

VariableWidthBitArray::VariableWidthBitArray(const std::string& pBaseName, FileFactory& pFactory)
    : mFile(pBaseName, pFactory)
{
}


