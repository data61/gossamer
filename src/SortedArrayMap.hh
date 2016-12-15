// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SORTEDARRAYMAP_HH
#define SORTEDARRAYMAP_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef STD_UTILITY
#include <utility>
#define STD_UTILITY
#endif

#ifndef STD_STDEXCEPT
#include <stdexcept>
#define STD_STDEXCEPT
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif

template<
    class K,
    class V
>
class MutableSortedArrayMap
{
public:
    typedef K key_type;
    typedef V data_type;

    typedef typename std::pair<K,V> value_type;

private:
    static const uint64_t sLogConstantFactor = 2;

    typedef std::vector<value_type> container_type;
    typedef typename container_type::iterator container_iterator;
    typedef typename std::iterator_traits<container_iterator>::difference_type
        difference_type;

public:
    V* find(const K& pItem)
    {
        for (container_iterator i = mOverflow.begin();
             i != mOverflow.end(); ++i)
        {
            if (i->first == pItem)
            {
                return &i->second;
            }
        }
        container_iterator first = mMain.begin(), last = mMain.end(), mid;
        difference_type len = std::distance(first, last), half;

        while (len > 0)
        {
            half = len >> 1;
            mid = first;
            std::advance(mid, half);
            if (mid->first < pItem)
            {
                first = mid;
                ++first;
                len -= half + 1;
            }
            else
            {
                len = half;
            }
        }

        return (first == last || first->first != pItem) ? 0 : &first->second;
    }

    V& insert(const K& pItem, const V& pValue)
    {
        if (mOverflow.size() >= mOverflow.capacity())
        {
            copyOverflowToMain();
        }
        mOverflow.push_back(value_type(pItem, pValue));
        return mOverflow.back().second;
    }

    V& operator[](const K& pItem)
    {
        V* value = find(pItem);
        if (value)
        {
            return *value;
        }
        else
        {
            return insert(pItem, data_type());
        }
    }

    MutableSortedArrayMap()
        : mOrder(4)
    {
        mMain.reserve(1ull << mOrder);
        mOverflow.reserve(sLogConstantFactor * mOrder);
    }


private:
    size_t mOrder;
    container_type mMain;
    container_type mOverflow;

    void copyOverflowToMain();
};


template<class K, class V>
void
MutableSortedArrayMap<K,V>::copyOverflowToMain()
{
    using namespace std;

    if (mOverflow.empty())
    {
        return;
    }

    size_t newSize = mMain.size() + mOverflow.size();
    size_t orderSize = size_t(1) << mOrder;
    if (newSize > orderSize)
    {
        ++mOrder;
        orderSize *= 2;
    }

    mMain.reserve(max(orderSize, newSize));
    if (newSize < 64)
    {
        copy(mOverflow.begin(), mOverflow.end(),
                back_insert_iterator<container_type>(mMain));
        sort(mMain.begin(), mMain.end());
    }
    else
    {
        sort(mOverflow.begin(), mOverflow.end());
        container_iterator mid = mMain.end();
        copy(mOverflow.begin(), mOverflow.end(),
                back_insert_iterator<container_type>(mMain));
        inplace_merge(mMain.begin(), mid, mMain.end());
    }

    mOverflow.clear();
    mOverflow.reserve(mOrder * sLogConstantFactor);
}


template<typename K, typename V,
         typename Compare = std::less<K>,
         typename Alloc = std::allocator< std::pair<K, V> >
         >
class SortedArrayMap
{
public:
    typedef K key_type;
    typedef V mapped_type;
    typedef std::pair<K, V> value_type;
    typedef Compare key_compare;
    typedef Alloc allocator_type;

private:
    typedef typename Alloc::value_type alloc_value_type;
    typedef std::vector< value_type, allocator_type > rep_type;

public:
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef typename rep_type::iterator iterator;
    typedef typename rep_type::const_iterator const_iterator;
    typedef typename rep_type::size_type size_type;
    typedef typename rep_type::difference_type difference_type;
    typedef typename rep_type::reverse_iterator reverse_iterator;
    typedef typename rep_type::const_reverse_iterator const_reverse_iterator;

    class value_compare
        : public std::binary_function<value_type, value_type, bool>
    {
        friend class SortedArrayMap<K, V, Compare, Alloc>;
    protected:
        Compare mComp;
   
        value_compare(Compare pComp)
            : mComp(pComp)
        {
        }

    public:
        bool operator()(const value_type& pX, const value_type& pY) const
        {
            return comp(pX.first, pY.first);
        }
    };

private:
    Gossamer::BaseOpt<key_compare, rep_type> mImpl;

    class value_key_compare
    {
        friend class SortedArrayMap<K, V, Compare, Alloc>;
    protected:
        Compare mComp;
   
        value_key_compare(Compare pComp)
            : mComp(pComp)
        {
        }

    public:
        bool operator()(const value_type& pV, const key_type& pK) const
        {
            return mComp(pV.first, pK);
        }

        bool operator()(const key_type& pK, const value_type& pV) const
        {
            return mComp(pK, pV.first);
        }

    };

    iterator
    _insert_at(iterator pPosition, const value_type& pValue)
    {
        return mImpl.m.insert(pPosition, pValue);
    }

    iterator
    _insert_between(iterator pBegin, iterator pEnd, const value_type& pValue)
    {
        iterator ii = Gossamer::tuned_lower_bound(pBegin, pEnd, pValue,
                value_key_compare(key_comp()));
        return _insert_at(ii, pValue);
    }

public:


    SortedArrayMap(const key_compare& pComp = key_compare(),
                   const allocator_type& pAlloc = allocator_type())
        : mImpl(pComp, rep_type(pAlloc))
    {
    }


    SortedArrayMap(const SortedArrayMap& pMap)
        : mImpl(pMap.mImpl)
    {
    }


    template<typename It>
    SortedArrayMap(It pFirst, It pLast)
        : mImpl()
    {
        insert_unique(pFirst, pLast);
    }


    template<typename It>
    SortedArrayMap(It pFirst, It pLast, const key_compare& pComp,
                   const allocator_type& pAlloc = allocator_type())
        : mImpl(pComp, rep_type(pAlloc))
    {
        insert_unique(pFirst, pLast);
    }


    allocator_type
    get_allocator() const
    {
        return mImpl.m.get_allocator();
    }


    key_compare
    key_comp() const
    {
        return mImpl;
    }


    value_compare
    value_comp() const
    {
        return value_compare(key_comp());
    }


    void swap(SortedArrayMap& pRhs)
    {
        mImpl.swap(pRhs.mImpl);
    }


    SortedArrayMap&
    operator=(const SortedArrayMap& pRhs)
    {
        SortedArrayMap newMap(pRhs);
        swap(newMap);
        return *this;
    }


    void reserve(size_type pEntries)
    {
        mImpl.m.reserve(pEntries);
    }


    size_type capacity() const
    {
        return mImpl.m.capacity();
    }


    iterator begin()
    {
        return mImpl.m.begin();
    }

    const_iterator begin() const
    {
        return mImpl.m.begin();
    }

    iterator end()
    {
        return mImpl.m.end();
    }

    const_iterator end() const
    {
        return mImpl.m.end();
    }

    reverse_iterator rbegin()
    {
        return mImpl.m.rbegin();
    }

    const_reverse_iterator rbegin() const
    {
        return mImpl.m.rbegin();
    }

    reverse_iterator rend()
    {
        return mImpl.m.rend();
    }

    const_reverse_iterator rend() const
    {
        return mImpl.m.rend();
    }

    bool empty() const
    {
        return mImpl.m.empty();
    }

    size_type size() const
    {
        return mImpl.m.size();
    }

    size_type max_size() const
    {
        return mImpl.m.max_size();
    }

    mapped_type&
    operator[](const key_type& pKey)
    {
        iterator ii = lower_bound(pKey);
        if (ii == end() || key_comp()(pKey, ii->first))
        {
            ii = _insert_at(ii, value_type(pKey, mapped_type()));
        }
        return ii->second;
    }

    mapped_type&
    at(const key_type& pKey)
    {
        iterator ii = lower_bound(pKey);
        if (ii == end() || key_comp()(pKey, ii->first))
        {
            BOOST_THROW_EXCEPTION(
                std::out_of_range("SortedArrayMap::at"));
        }
        return ii->second;
    }

    const mapped_type&
    at(const key_type& pKey) const
    {
        iterator ii = lower_bound(pKey);
        if (ii == end() || key_comp()(pKey, ii->first))
        {
            BOOST_THROW_EXCEPTION(
                std::out_of_range("SortedArrayMap::at"));
        }
        return ii->second;
    }

    std::pair<iterator,bool>
    insert(const value_type& pValue)
    {
        iterator ii = lower_bound(pValue.first);
        if (ii == end() || key_comp()(pValue.first, ii->first))
        {
            return make_pair(_insert_at(ii, pValue), true);
        }
        return make_pair(ii, false);
    }

    iterator insert(iterator pPosition, const value_type& pValue);

    template<typename It>
    void
    insert(It pBegin, It pEnd)
    {
        for (; pBegin != pEnd; ++pBegin)
        {
            insert(end(), *pBegin);
        }
    }

#if 0
    void erase(iterator pPos);
    size_type erase(const key_type& pKey);
    void erase(iterator pBegin, iterator pEnd);
    void clear();
#endif

    iterator find(const key_type& pKey)
    {
        iterator ii = lower_bound(pKey);
        if (ii == end() || key_comp()(pKey, ii->first))
        {
            return end();
        }
        return ii;
    }

    const_iterator find(const key_type& pKey) const
    {
        const_iterator ii = lower_bound(pKey);
        if (ii == end() || key_comp()(pKey, ii->first))
        {
            return end();
        }
        return ii;
    }

    size_type count(const key_type& pKey) const
    {
        return find(pKey) == end() ? 0 : 1;
    }


    iterator lower_bound(const key_type& pKey)
    {
        using namespace Gossamer;
        return tuned_lower_bound<iterator,key_type,value_key_compare,64>
            (begin(), end(), pKey, value_key_compare(key_comp()));
    }

    const_iterator lower_bound(const key_type& pKey) const
    {
        using namespace Gossamer;
        return tuned_lower_bound<const_iterator,key_type, value_key_compare,64>
            (begin(), end(), pKey, value_key_compare(key_comp()));
    }


#if 0
    iterator upper_bound(const key_type& pKey);
    const_iterator upper_bound(const key_type& pKey) const:
    std::pair<iterator,iterator> equal_range(const key_type& pKey);
    std::pair<const_iterator,const_iterator> equal_range(const key_type& pKey) const;

    template<typename K, typename V, typename C, typename A>
    inline bool
    operator==(const SortedArrayMap<K, V, C, A>& pX,
               const SortedArrayMap<K, V, C, A>& pY);

    template<typename K, typename V, typename C, typename A>
    inline bool
    operator<(const SortedArrayMap<K, V, C, A>& pX,
               const SortedArrayMap<K, V, C, A>& pY);

#endif
};


template<typename K, typename V, typename C, typename A>
typename SortedArrayMap<K,V,C,A>::iterator
SortedArrayMap<K,V,C,A>::insert(iterator pPosition,
                                const value_type& pValue)
{
    const key_type& key = pValue.first;

    if (pPosition == end())
    {
        // The hint was at the end.

        if (mImpl.m.empty() || key_comp()(mImpl.m.back().first, key))
        {
            // We can do this if the map is empty or the key is greater
            // than the last item.
            return _insert_at(end(), pValue);
        }
        else
        {
            return insert(pValue).first;
        }
    }
    else if (key_comp()(key, pPosition->first))
    {
        // The key is less than the key at pPosition.  Check the
        // previous item (if it exists) to see if we can insert here.
        iterator before = pPosition;
        if (pPosition == begin() || key_comp()((--before)->first, key))
        {
            return _insert_at(pPosition, pValue);
        }
        else
        {
            return _insert_between(begin(), pPosition, pValue);
        }
    }
    else if (key_comp()(pPosition->first, key))
    {
        // The key is greater than the key at pPosition.  Check the
        // next item (if it exists) to see if we can insert there.
        iterator after = pPosition;
        ++after;
        if (after == end() || key_comp()(key, after->first))
        {
            return _insert_at(after, pValue);
        }
        else
        {
            return _insert_between(begin(), pPosition, pValue);
        }
    }
    else
    {
        // Equivalent keys.
        return pPosition;
    }
}

#endif // SORTEDARRAYMAP_HH
