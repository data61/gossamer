// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PRIORITYSEARCHQUEUE_HH
#define PRIORITYSEARCHQUEUE_HH

#ifndef BOOST_MULTI_INDEX_CONTAINER_HPP
#include <boost/multi_index_container.hpp>
#define BOOST_MULTI_INDEX_CONTAINER_HPP
#endif

#ifndef BOOST_MULTI_INDEX_ORDERED_INDEX_HPP
#include <boost/multi_index/ordered_index.hpp>
#define BOOST_MULTI_INDEX_ORDERED_INDEX_HPP
#endif

#ifndef BOOST_MULTI_INDEX_HASHED_INDEX_HPP
#include <boost/multi_index/hashed_index.hpp>
#define BOOST_MULTI_INDEX_HASHED_INDEX_HPP
#endif

#ifndef BOOST_MULTI_INDEX_MEMBER_HPP
#include <boost/multi_index/member.hpp>
#define BOOST_MULTI_INDEX_MEMBER_HPP
#endif

#ifndef BOOST_CALL_TRAITS_HPP
#include <boost/call_traits.hpp>
#define CALL_TRAITS_HPP
#endif

template<typename K, typename V,
         typename KeyHash = std::hash<K>,
         typename KeyPred = std::equal_to<K>,
         typename ValueComp = std::less<V>,
         typename Allocator = std::allocator<char>
    >
class PrioritySearchQueue
{
public:
    typedef K key_type;
    typedef V data_type;
    typedef std::pair<K, V> value_type;
    typedef KeyHash key_hash;
    typedef KeyPred key_pred;
    typedef ValueComp value_compare;
    typedef std::size_t size_type;
    typedef Allocator allocator_type;

    typedef typename boost::call_traits<K>::param_type key_param_type;
    typedef typename boost::call_traits<V>::reference data_reference;

private:
    struct key_name {};
    typedef boost::multi_index::tag<key_name> key_tag;

    struct value_name {};
    typedef boost::multi_index::tag<value_name> value_tag;

    typedef boost::multi_index::member<value_type, K, &value_type::first>
        key_extractor;
    typedef boost::multi_index::member<value_type, V, &value_type::second>
        value_extractor;

    typedef typename Allocator::template rebind<value_type>::other
        value_alloc_type;

    typedef boost::multi_index_container<
        value_type,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<key_tag,key_extractor,KeyHash,KeyPred>,
            boost::multi_index::ordered_non_unique<value_tag,value_extractor,ValueComp>
        >,
        value_alloc_type
    > rep_type;

    rep_type mRep;

    typedef typename boost::multi_index::index<rep_type,key_name>::type key_index;
    typedef typename boost::multi_index::index<rep_type,value_name>::type value_index;

    const key_index& getKeyIndex() const
    {
        return boost::multi_index::get<key_name>(mRep);
    }

    key_index& getKeyIndex()
    {
        return boost::multi_index::get<key_name>(mRep);
    }

    const value_index& getValueIndex() const
    {
        return boost::multi_index::get<value_name>(mRep);
    }

    value_index& getValueIndex()
    {
        return boost::multi_index::get<value_name>(mRep);
    }

public:
    typedef typename key_index::iterator iterator;
    typedef typename key_index::const_iterator const_iterator;

    PrioritySearchQueue()
    {
    }

    PrioritySearchQueue(const PrioritySearchQueue& pRhs)
        : mRep(pRhs.mRep)
    {
    }

    bool empty() const
    {
        return mRep.empty();
    }

    size_type size() const
    {
        return mRep.size();
    }

    const value_type& top() const
    {
        return *getValueIndex().begin();
    }

    void pop()
    {
        value_index& vindex = getValueIndex();
        vindex.erase(vindex.begin());
    }

    void push(const value_type& pValue)
    {
        mRep.insert(pValue);
    }

    iterator begin()
    {
        return getKeyIndex().begin();
    }

    const_iterator begin() const
    {
        return getKeyIndex().begin();
    }

    iterator end()
    {
        return getKeyIndex().end();
    }

    const_iterator end() const
    {
        return getKeyIndex().begin();
    }

    void clear()
    {
        getKeyIndex().clear();
    }

    void erase(iterator pPos)
    {
        getKeyIndex().erase(pPos);
    }

    void erase(iterator pFirst, iterator pLast)
    {
        getKeyIndex().erase(pFirst, pLast);
    }

    iterator find(key_param_type pK)
    {
        return getKeyIndex().find(pK);
    }

    const_iterator find(key_param_type pK) const
    {
        return getKeyIndex().find(pK);
    }

    size_type count(key_param_type pK) const
    {
        return getKeyIndex().count(pK);
    }

    iterator lower_bound(key_param_type pK)
    {
        return getKeyIndex().lower_bound(pK);
    }

    const_iterator lower_bound(key_param_type pK) const
    {
        return getKeyIndex().lower_bound(pK);
    }

    iterator upper_bound(key_param_type pK)
    {
        return getKeyIndex().upper_bound(pK);
    }

    const_iterator upper_bound(key_param_type pK) const
    {
        return getKeyIndex().upper_bound(pK);
    }

    data_reference operator[](key_param_type pK)
    {
        return getKeyIndex()[pK];
    }

};

#endif // PRIORITYSEARCHQUEUE_HH
