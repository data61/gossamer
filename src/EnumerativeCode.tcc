
#ifndef UTILS_HH
#include "Utils.hh"
#endif

template<int Bits>
EnumerativeCode<Bits>::EnumerativeCode()
{
    uint64_t previ = 0;
    uint64_t i = 0;
    for (uint64_t n = 0; n <= s_wordBits; ++n)
    {
        mChoose[i] = 1;
        mChoose[i + n] = 1;
        for (uint64_t k = 1; k < n; ++k)
        {
            mChoose[i + k] = mChoose[previ + k - 1] + mChoose[previ + k];
        }
        previ = i;
        i += n+1;
    }
    for (i = 0, previ = s_wordBits*(s_wordBits+1)/2; i < s_wordBits+1;
            ++i, ++previ)
    {
        mChooseLog2[i] = Gossamer::log2(mChoose[previ]);
    }
}

