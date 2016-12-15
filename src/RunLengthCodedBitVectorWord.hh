// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef RUNLENGTHCODEDBITVECTORWORD_HH
#define RUNLENGTHCODEDBITVECTORWORD_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_UTILITY
#include <utility>
#define STD_UTILITY
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef STD_STDEXCEPT
#include <stdexcept>
#define STD_STDEXCEPT
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

#ifndef TRIVIALVECTOR_HH
#include "TrivialVector.hh"
#endif

template <typename Codec>
class RunLengthCodedBitVectorWord
{
public:
    static uint64_t size(uint64_t w)
    {
        w >>= 1;
        uint64_t r = 0;
        while (w)
        {
            r += Codec::decode(w);
        }
        return r;
    }

    static uint64_t count(uint64_t w)
    {
        uint64_t k = w & 1;
        w >>= 1;
        uint64_t r = 0;
        while (w)
        {
            r += k * Codec::decode(w);
            k = 1 - k;
        }
        return r;
    }

    static uint64_t bits(uint64_t w)
    {
        uint64_t n = 0;
        w >>= 1;
        while (w)
        {
            uint64_t x = Codec::decode(w);
            uint64_t y = 0;
            n += Codec::encode(x, y);
        }
        if (n > 0)
        {
            ++n;
        }
        return n;
    }

    static std::pair<uint64_t,uint64_t> sizeAndCount(uint64_t w)
    {
        uint64_t k = w & 1;
        w >>= 1;
        uint64_t s = 0;
        uint64_t c = 0;
        while (w)
        {
            uint64_t r = Codec::decode(w);
            s += r;
            c += k * r;
            k = 1 - k;
        }
        return std::pair<uint64_t,uint64_t>(s, c);
    }

    static bool access(uint64_t w, uint64_t p)
    {
        return rank(w, p + 1) - rank(w, p);
    }

    static uint64_t rank(uint64_t w, uint64_t p)
    {
        uint64_t k = w & 1;
        w >>= 1;
        uint64_t c = 0;
        uint64_t s = 0;
        while (w)
        {
            uint64_t l = Codec::decode(w);
            if (s + l >= p)
            {
                return c + k * (p - s);
            }
            c += k * l;
            s += l;
            k = 1 - k;
        }
        return c;
    }

    static uint64_t select(uint64_t w, uint64_t r)
    {
        uint64_t k = w & 1;
        w >>= 1;
        uint64_t c = 0;
        uint64_t s = 0;
        while (w)
        {
            uint64_t l = Codec::decode(w);
            if (c + k * l > r)
            {
                return s + (r - c);
            }
            c += k * l;
            s += l;
            k = 1 - k;
        }
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::range_error("RunLengthCodedBitVectorWord::select", c, r));
    }

    static uint64_t insert(uint64_t& w, uint64_t p, bool b)
    {
        //std::cerr << "insert " << w << '\t' << p << '\t' << b << std::endl;

        const bool s0 = !(w & 1);
        bool s = !(w & 1);
        w >>= 1;

        if (!w)
        {
            BOOST_ASSERT(p == 0);
            w = 2 | b;
            return 0;
        }

        uint64_t c = 0;
        uint64_t x = 0;
        TrivialVector<uint64_t,65> xs;
        while (w)
        {
            uint64_t y = Codec::decode(w);
            if (p <= c + x && s == b)
            {
                xs.push_back(x + 1);
                xs.push_back(y);
                return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s0);
            }
            if (p < c + x && s != b)
            {
                xs.push_back(p - c);
                xs.push_back(1);
                xs.push_back((c + x) - p);
                xs.push_back(y);
                return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s0);
            }
            if (p == c + x && s != b)
            {
                xs.push_back(x);
                xs.push_back(y + 1);
                return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s0);
            }

            xs.push_back(x);

            s = !s;
            c += x;
            x = y;
        }

        if (p <= c + x && s == b)
        {
            xs.push_back(x + 1);
        }
        else if (p < c + x && s != b)
        {
            xs.push_back(p - c);
            xs.push_back(1);
            xs.push_back((c + x) - p);
        }
        else if (p == c + x && s != b)
        {
            xs.push_back(x);
            xs.push_back(1);
        }

        return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s0);
    }

    static uint64_t erase(uint64_t& w, uint64_t p)
    {
        TrivialVector<uint64_t,65> xs;
        bool s = w & 1;
        w >>= 1;
        uint64_t x = Codec::decode(w);
        if (!w)
        {
            BOOST_ASSERT(p < x);
            uint64_t r = 0;
            if (x > 1)
            {
                Codec::encode(x - 1, r);
            }
            r = (r << 1) | s;
            w = r;
            return 0;
        }

        uint64_t y = Codec::decode(w);
        uint64_t c = 0;
        if (p < x)
        {
            if (x > 1)
            {
                xs.push_back(x - 1);
                xs.push_back(y);
            }
            else
            {
                xs.push_back(y);
                s = !s;
            }
            return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s);
        }

        c = x;

        while (w)
        {
            uint64_t z = Codec::decode(w);
            if (p < c + y)
            {
                if (y > 1)
                {
                    xs.push_back(x);
                    xs.push_back(y - 1);
                    xs.push_back(z);
                }
                else
                {
                    xs.push_back(x + z);
                }
                return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s);
            }
            xs.push_back(x);
            c += y;
            x = y;
            y = z;
        }

        if (p < c + y)
        {
            if (y > 1)
            {
                xs.push_back(x);
                xs.push_back(y - 1);
            }
            else
            {
                xs.push_back(x);
            }
            return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s);
        }

        c += y;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::range_error("RunLengthCodedBitVectorWord::erase", c, p));
    }

    static uint64_t append(uint64_t& w, uint64_t p, bool b)
    {
        TrivialVector<uint64_t,65> xs;
        const bool s = w & 1;
        w >>= 1;

        if (w == 0)
        {
            xs.push_back(p);
            return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, b);
        }
        while (w)
        {
            uint64_t z = Codec::decode(w);
            xs.push_back(z);
        }
        bool e = ((xs.size() & 1) ? s : !s);
        if (e == b)
        {
            xs.back() += p;
        }
        else
        {
            xs.push_back(p);
        }
        return RunLengthCodedBitVectorWord<Codec>::recode(w, xs, s);
    }

    static uint64_t merge(uint64_t& lhs, uint64_t rhs)
    {
        std::vector<std::pair<uint64_t,uint64_t> > ls;
        ones(lhs, ls);

        std::vector<std::pair<uint64_t,uint64_t> > rs;
        ones(rhs, rs);

        std::vector<std::pair<uint64_t,uint64_t> > xs;
        uint64_t l = 0;
        uint64_t r = 0;
        while (l < ls.size() && r < rs.size())
        {
            if (ls[l].second < rs[r].first)
            {
                xs.push_back(ls[l]);
                ++l;
                continue;
            }
            if (rs[r].second < ls[l].first)
            {
                xs.push_back(rs[r]);
                ++r;
                continue;
            }
            if (ls[l].first <= rs[r].first)
            {
                ls[l].second = std::max(ls[l].second, rs[r].second);
                ++r;
                continue;
            }
            else
            {
                rs[r].second = std::max(ls[l].second, rs[r].second);
                ++l;
                continue;
            }
        }
        if (xs.empty())
        {
            if (l < ls.size())
            {
                xs = ls;
            }
            else
            {
                xs = rs;
            }
        }
        else
        {
            while (l < ls.size())
            {
                if (ls[l].first <= xs.back().second)
                {
                    xs.back().second = std::max(xs.back().second, ls[l].second);
                }
                else
                {
                    xs.push_back(ls[l]);
                }
                ++l;
            }
            while (r < rs.size())
            {
                if (rs[r].first <= xs.back().second)
                {
                    xs.back().second = std::max(xs.back().second, rs[r].second);
                }
                else
                {
                    xs.push_back(rs[r]);
                }
                ++r;
            }
        }
        if (xs.size() == 0)
        {
            lhs = 0;
            return 0;
        }
        TrivialVector<uint64_t,65> ys;
        bool s = (xs.front().first == 0);
        if (xs.front().first != 0)
        {
            ys.push_back(xs.front().first);
        }
        for (uint64_t i = 0; i < xs.size(); ++i)
        {
            ys.push_back(xs[i].second - xs[i].first);
            if (i + 1 < xs.size())
            {
                ys.push_back(xs[i + 1].first - xs[i].second);
            }
        }
        return RunLengthCodedBitVectorWord<Codec>::recode(lhs, ys, s);
    }

    static void dump(uint64_t pWord, std::ostream& pOut)
    {
        bool k = pWord & 1;
        pWord >>= 1;
        while (pWord)
        {
            uint64_t x = Codec::decode(pWord);
            pOut << '<' << k << ',' << x << '>';
            k = !k;
        }
    }

    /**
     * Initialize a word to a homogoneous sequence of pLen pVals.
     * Returns the number of bits required for the encoding.
     */
    static uint64_t init(uint64_t& pWord, uint64_t pLen, bool pVal)
    {
        pWord = 0;
        uint64_t l = Codec::encode(pLen, pWord);
        pWord = (pWord << 1) | pVal;
        return l + 1;
    }

private:
    static void ones(uint64_t& pWord, std::vector<std::pair<uint64_t,uint64_t> >& pOnes)
    {
        bool s = pWord & 1;
        pWord >>= 1;
        uint64_t p = 0;
        while (pWord)
        {
            uint64_t x = Codec::decode(pWord);
            if (s)
            {
                pOnes.push_back(std::pair<uint64_t,uint64_t>(p, p + x));
            }
            p += x;
            s = !s;
        }
    }

    static uint64_t recode(uint64_t& pWord, TrivialVector<uint64_t,65>& pItems, bool pSense)
    {
        while (pWord)
        {
            pItems.push_back(Codec::decode(pWord));
        }

        uint64_t w0 = 0;
        uint64_t w1 = 0;

        bool s = pSense;
        uint64_t i = 0;
        BOOST_ASSERT(pItems.size() > 0);
        //std::cerr << "items: " << std::endl;
        //for (uint64_t i = 0; i < pItems.size(); ++i)
        //{
        //    std::cerr << '\t' << i << '\t' << pItems[i] << std::endl;
        //}
        if (pItems[0] == 0)
        {
            i = 1;
            s = !s;
        }
        uint64_t wx = s;
        uint64_t z = 1;
        for (; i < pItems.size(); ++i, s = !s)
        {
            uint64_t x = 0;
            uint64_t l = Codec::encode(pItems[i], x);
            if (z + l > 64)
            {
                BOOST_ASSERT(w0 == 0);
                w0 = wx;
                wx = s;
                z = 1;
            }
            wx |= x << z;
            z += l;
        }
        if (w0)
        {
            w1 = wx;
        }
        else
        {
            w0 = wx;
        }
        pWord = w0;
        return w1;
    }
};

#endif // RUNLENGTHCODEDBITVECTORWORD_HH
