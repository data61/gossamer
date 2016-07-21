#include "WordyBitVector.hh"

WordyBitVector::Builder::Builder(const std::string& pName, FileFactory& pFactory)
    : mFile(pName, pFactory),
      mCurrPos(0), mFileWordNum(0), mCurrWordNum(0), mCurrWord(0)
{
}


void
WordyBitVector::Builder::flush()
{
    while (mFileWordNum < mCurrWordNum)
    {
        uint64_t zero = 0;
        mFile.push_back(zero);
        ++mFileWordNum;
    }
    mFile.push_back(mCurrWord);
    ++mFileWordNum;
}


// Constructor
//
WordyBitVector::WordyBitVector(const std::string& pName, FileFactory& pFactory)
    : mWords(pName, pFactory)
{
}


// Count the number of 1 bits in the range [pBegin,pEnd).
//
uint64_t
WordyBitVector::popcountRange(uint64_t pBegin, uint64_t pEnd) const
{
    uint64_t wb = pBegin / wordBits;
    uint64_t bb = pBegin % wordBits;
    uint64_t we = pEnd / wordBits;
    uint64_t be = pEnd % wordBits;
    if (pBegin >= pEnd || wb > words())
    {
        return 0;
    }
    if (we >= words())
    {
        we = words() - 1;
        be = 64;
    }

    uint64_t beginMask = ~uint64_t(0) << bb;
    uint64_t endMask = ~uint64_t(0) >> (64 - be);

    if (wb == we)
    {
        return Gossamer::popcnt(mWords[wb] & beginMask & endMask);
    }

    uint64_t rank = Gossamer::popcnt(mWords[wb] & beginMask);
    for (uint64_t i = wb + 1; i < we; ++i)
    {
        rank += Gossamer::popcnt(mWords[i]);
    }
    if (be > 0)
    {
        rank += Gossamer::popcnt(mWords[we] & endMask);
    }
    return rank;
}

