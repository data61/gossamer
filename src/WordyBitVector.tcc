#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif


// Find the position of the 'pCount'th bit
// relative to the 'pFrom'th bit.
//
template<typename Sense>
uint64_t
WordyBitVector::select(uint64_t pFrom, uint64_t pCount) const
{
    uint64_t w = pFrom / wordBits;
    uint64_t b = pFrom % wordBits;

    if (w >= words())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::range_error("WordyBitVector::select", words(), w));
    }
        
    // First locate the right word.
    //
    uint64_t c = pCount;
    uint64_t x = (Sense::sInvert ? ~mWords[w] : mWords[w]) >> b;
    uint64_t p = Gossamer::popcnt(x);
    while (c >= p)
    {
        c -= p;
        ++w;
        b = 0;
#ifndef NDEBUG
        if (w >= words())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::range_error("WordyBitVector::select", words(), w));
        }
#endif
        x = (Sense::sInvert ? ~mWords[w] : mWords[w]);
        p = Gossamer::popcnt(x);
    }

    return (w * wordBits) + b + Gossamer::select1(x, c);
}


