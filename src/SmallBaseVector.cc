// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "SmallBaseVector.hh"


std::ostream&
SmallBaseVector::print(std::ostream& pOut, uint64_t pLineLength) const
{
    for (uint64_t i = 0; i < mSize; i += pLineLength)
    {
        uint64_t upper = std::min(i + pLineLength, mSize);
        for (uint64_t j = i; j < upper; ++j)
        {
            static const char* map = "ACGT";
            pOut << map[(*this)[j]];
        }
        pOut << '\n';
    }
    return pOut;
}


void
SmallBaseVector::write(std::ostream& pOut) const
{
    //cerr << "writing " << (*this) << endl;
    pOut.write(reinterpret_cast<const char*>(&mSize), sizeof(mSize));
    BOOST_ASSERT(mBases.size() == (mSize + sBasesPerWord - 1) / sBasesPerWord);
    pOut.write(reinterpret_cast<const char*>(&mBases[0]),
               mBases.size() * sizeof(uint16_t));
}


bool
SmallBaseVector::read(std::istream& pIn)
{
    pIn.read(reinterpret_cast<char*>(&mSize), sizeof(mSize));
    if (!pIn.good())
    {
        clear();
        return false;
    }
    uint64_t n = (mSize + sBasesPerWord - 1) / sBasesPerWord;
    mBases.resize(n);
    pIn.read(reinterpret_cast<char*>(&mBases[0]), n * sizeof(uint16_t));
    if (!pIn.good())
    {
        clear();
        return false;
    }
    //cerr << "read " << (*this) << endl;
    return true;
}


bool
SmallBaseVector::make(const std::string& pStr, SmallBaseVector& pRead)
{
    bool succeded = true;
    pRead.reserve(pRead.size() + pStr.size());
    for (uint64_t i = 0; i < pStr.size(); ++i)
    {
        switch (pStr[i])
        {
            case 'A':
            case 'a':
            {
                pRead.push_back(0);
                break;
            }
            case 'C':
            case 'c':
            {
                pRead.push_back(1);
                break;
            }
            case 'G':
            case 'g':
            {
                pRead.push_back(2);
                break;
            }
            case 'T':
            case 't':
            {
                pRead.push_back(3);
                break;
            }
            default:
            {
                succeded = false;
                break;
            }
        }
    }
    return succeded;
}


size_t
SmallBaseVector::editDistance(const SmallBaseVector& pRhs) const
{
    // A SmallBaseVector is Small, so O(n^2) is probably fine.

    // TODO If we know a maximum edit distance, we should quit early.
    size_t m = size();
    size_t n = pRhs.size();
    std::vector<size_t> a;
    a.resize(m+1);
    std::vector<size_t> b;
    b.resize(m+1);

    for (size_t i = 0; i <= m; ++i)
    {
        a[i] = i;
    }

    for (size_t j = 1; j <= n; ++j)
    {
        b[0] = j;
        for (size_t i = 1; i <= m; ++i)
        {
            if ((*this)[i-1] == pRhs[j-1])
            {
                b[i] = a[i-1];
            }
            else
            {
                b[i] = std::min(std::min(a[i], a[i-1]), b[i-1]) + 1;
            }
        }
        std::swap(a, b);
    }
    return a[m];
}


