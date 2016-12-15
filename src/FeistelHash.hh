// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef FEISTELHASH_HH
#define FEISTELHASH_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_UTILITY
#include <utility>
#define STD_UTILITY
#endif

class FeistelHash
{
public:
    static const uint64_t J = 2;

    typedef uint64_t value_type;
    typedef std::pair<value_type,value_type> Item;

public:
    static Item hash(const Item& pItem)
    {
        Item tmp = pItem;
        for (uint64_t j = 0; j < J; ++j)
        {
            tmp = Item(tmp.second, tmp.first ^ univ(j, tmp.second));
        }
        return tmp;
    }

    static Item unhash(const Item& pItem)
    {
        Item tmp = pItem;
        for (uint64_t i = 0; i < J; ++i)
        {
            uint64_t j = J - i - 1;
            tmp = Item(tmp.second ^ univ(j, tmp.first), tmp.first);
        }
        return tmp;
    }

private:

    static uint64_t univ(uint64_t j, uint64_t x)
    {
        static const uint64_t as[] = {
             12203532102539482277ULL, 12369422297701164583ULL,
        };

        static const uint64_t bs[] = {
             15859037850348216889ULL, 12573556904978588377ULL,
        };

        return as[j] * x + bs[j];
    }
};

#endif // FEISTELHASH_HH
