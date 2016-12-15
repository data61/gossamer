// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ARRAYMAP_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

template<
    class K,
    class V,
    class EqualKey = std::equal_to<K>,
    class Container = std::vector< std::pair<K,V> >
>
class ArrayMap : private Container
{
private:
    typedef Container container_type;
public:
    typedef K key_type;
    typedef V data_type;
    typedef EqualKey key_equal;
    typedef typename std::pair<K,V> value_type;

    typedef value_type* pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;

    typedef typename container_type::size_type size_type;
    typedef typename container_type::difference_type difference_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;

    key_equal key_eq() const
    {
        return mKeyEqual;
    }

    ArrayMap()
    {
    }

    ArrayMap(const ArrayMap& pRhs)
        : Container(pRhs), mKeyEqual(pRhs.mKeyEqual)
    {
    }

    ArrayMap(const key_equal& pKeyEq)
        : mKeyEqual(pKeyEq)
    {
    }

    template<class InputIterator>
    ArrayMap(InputIterator pBegin, InputIterator pEnd)
        : Container(pBegin, pEnd)
    {
    }

    template<class InputIterator>
    ArrayMap(InputIterator pBegin, InputIterator pEnd, const key_equal& pKeyEq)
        : Container(pBegin, pEnd), mKeyEqual(pKeyEq)
    {
    }

    void swap(ArrayMap& pRhs)
    {
        container_type::swap(pRhs);
        std::swap(mKeyEqual, pRhs.mKeyEqual);
    }

    ArrayMap& operator=(const ArrayMap& pRhs)
    {
        ArrayMap map(pRhs);
        swap(map);
        return *this;
    }

    using container_type::begin;
    using container_type::end;
    using container_type::size;
    using container_type::max_size;
    using container_type::empty;
    using container_type::resize;
    using container_type::reserve;

    const_iterator
    find(const key_type& pKey) const
    {
        for (const_iterator i = begin(); i != end(); ++i)
        {
            if (mKeyEqual(i->first, pKey))
            {
                return i;
            }
        }
        return end();
    }

    iterator
    find(const key_type& pKey)
    {
        for (iterator i = begin(); i != end(); ++i)
        {
            if (mKeyEqual(i->first, pKey))
            {
                return i;
            }
        }
        return end();
    }

    std::pair<iterator,bool>
    insert(const value_type& pValue)
    {
        iterator i = find(pValue.first);
        if (i != end())
        {
            return std::pair<iterator,bool>(i, false);
        }
        else
        {
            i = container_type::insert(end(), pValue);
            return std::pair<iterator,bool>(i, true);
        }
    }

    size_type
    erase(iterator pPos)
    {
        container_type::erase(pPos);
        return size();
    }

    size_type
    erase(const key_type& pKey)
    {
        iterator i = find(pKey);
        if (i != end())
        {
            erase(i);
        }
        return size();
    }

    data_type&
    operator[](const key_type& pKey)
    {
        iterator i = find(pKey);
        if (i != end())
        {
            return i->second;
        }
        push_back(value_type(pKey, data_type()));
        return container_type::back().second;
    }

private:
    key_equal mKeyEqual;
};

#endif // ARRAYMAP_HH
