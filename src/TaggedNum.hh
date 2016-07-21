#ifndef TAGGEDNUM_HH
#define TAGGEDNUM_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef BOOST_OPERATORS_HPP
#include <boost/operators.hpp>
#define BOOST_OPERATORS_HPP
#endif

#ifndef BOOST_CALL_TRAITS_HPP
#include <boost/call_traits.hpp>
#define BOOST_CALL_TRAITS_HPP
#endif

#ifndef BOOST_UNORDERED_SET_HPP
#include <boost/unordered_set.hpp>
#define BOOST_UNORDERED_SET_HPP
#endif

template <typename Tag, typename ValueType = uint64_t>
class TaggedNum
    : public boost::equality_comparable<TaggedNum<Tag,ValueType>
           , boost::less_than_comparable<TaggedNum<Tag,ValueType>
           > >
{
public:
    typedef ValueType value_type;

    typedef typename boost::call_traits<value_type>::value_type value_return_type;
    typedef typename boost::call_traits<value_type>::param_type value_param_type;
    typedef typename boost::call_traits<value_type>::reference value_reference;

    struct Hash
    {
        size_t operator() (TaggedNum<Tag,ValueType> pTaggedNum) const
        {
            return boost::hash<typename TaggedNum<Tag,ValueType>::value_type>()(
                pTaggedNum.value());
        }
    };

    value_return_type value() const
    {
        return mValue;
    }

    value_reference value()
    {
        return mValue;
    }

    bool operator<(TaggedNum<Tag,ValueType> pRhs) const
    {
        return mValue < pRhs.mValue;
    }

    bool operator==(TaggedNum<Tag,ValueType> pRhs) const
    {
        return mValue == pRhs.mValue;
    }

    TaggedNum()
        : mValue(value_param_type())
    {
    }

    explicit TaggedNum(value_param_type pValue)
        : mValue(pValue)
    {
    }

private:
    ValueType mValue;
};

#endif // TAGGEDNUM_HH
