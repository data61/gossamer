// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "IntegerArray.hh"
#include "StackedArray.hh"

using namespace boost;
using namespace std;

template<typename StoreType>
class IntegerArrayBasicBuilder : public IntegerArray::Builder
{
public:
    typedef StoreType store_type;

    void push_back(value_type pItem)
    {
        mBuilder.push_back(static_cast<store_type>(pItem.asUInt64()));
    }

    void end()
    {
        mBuilder.end();
    }

    IntegerArrayBasicBuilder(const string& pBaseName, FileFactory& pFactory)
        : mBuilder(pBaseName, pFactory)
    {
    }

private:
    typename MappedArray<store_type>::Builder mBuilder;
};


namespace {

    void tryRemove(const string& pBaseName, FileFactory& pFactory, uint64_t pDepthLeft)
    {
        if (!pDepthLeft)
        {
            return;
        }

        try
        {
            pFactory.remove(pBaseName);
            return;
        }
        catch (Gossamer::error& e)
        {
            tryRemove(pBaseName + ".lwr", pFactory, pDepthLeft - 1);
            tryRemove(pBaseName + ".upr", pFactory, pDepthLeft - 1);
        }
    }
}


template<typename StoreType>
class IntegerArrayBasic : public IntegerArray
{
public:
    typedef StoreType store_type;
    static constexpr unsigned BinarySearchCutoff = 32;

    uint64_t size() const
    {
        return mArray.size();
    }

    value_type operator[](uint64_t pIdx) const
    {
        return value_type(mArray[pIdx]);
    }

    uint64_t lower_bound(uint64_t pBegin, uint64_t pEnd, const value_type& pVal) const
    {
        using namespace Gossamer;
        store_type value = static_cast<store_type>(pVal.asUInt64());
        const store_type* arr = mArray.begin();
        auto lb
            = tuned_lower_bound<
                    const store_type*,
                    store_type,
                    std::less<store_type>,
                    BinarySearchCutoff>
                (arr + pBegin, arr + pEnd, value, std::less<store_type>());
        return lb - arr;
    }

    uint64_t upper_bound(uint64_t pBegin, uint64_t pEnd, const value_type& pVal) const
    {
        using namespace Gossamer;
        store_type value = static_cast<store_type>(pVal.asUInt64());
        const store_type* arr = mArray.begin();
        auto ub
            = tuned_upper_bound<
                    const store_type*,
                    store_type,
                    std::less<store_type>,
                    BinarySearchCutoff>
                (arr + pBegin, arr + pEnd, value, std::less<store_type>());
        return ub - arr;
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putProp("storage", sizeof(store_type) * mArray.size());
        return t;
    }

    IntegerArrayBasic(const string& pBaseName, FileFactory& pFactory)
        : mArray(pBaseName, pFactory)
    {
    }

private:
    MappedArray<store_type> mArray;
};


template<typename UprType, typename LwrType>
class IntegerArrayStackedBuilder : public IntegerArray::Builder
{
public:
    typedef UprType upr_type;
    typedef LwrType lwr_type;

    void push_back(value_type pItem)
    {
        mBuilder.push_back(pItem);
    }

    void end()
    {
        mBuilder.end();
    }

    IntegerArrayStackedBuilder(const string& pBaseName, FileFactory& pFactory)
        : mBuilder(pBaseName, pFactory)
    {
    }

private:
    typename StackedArray<upr_type,lwr_type>::Builder mBuilder;
};


template<typename UprType, typename LwrType>
class IntegerArrayStacked : public IntegerArray
{
public:
    typedef UprType upr_type;
    typedef LwrType lwr_type;

    uint64_t size() const
    {
        return mArray.size();
    }

    uint64_t lower_bound(uint64_t pBegin, uint64_t pEnd, const value_type& pVal) const
    {
        return mArray.lower_bound(pBegin, pEnd, pVal);
    }

    uint64_t upper_bound(uint64_t pBegin, uint64_t pEnd, const value_type& pVal) const
    {
        return mArray.upper_bound(pBegin, pEnd, pVal);
    }

    value_type operator[](uint64_t pIdx) const
    {
        return value_type(mArray[pIdx]);
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putProp("storage", (sizeof(upr_type) + sizeof(lwr_type)) * mArray.size());
        return t;
    }

    IntegerArrayStacked(const string& pBaseName, FileFactory& pFactory)
        : mArray(pBaseName, pFactory)
    {
    }

private:
    StackedArray<upr_type,lwr_type> mArray;
};


template<typename StoreType>
class LazyIteratorBasic : public IntegerArray::LazyIterator
{
public:
    typedef StoreType store_type;

    virtual bool valid() const
    {
        return mItr.valid();
    }

    virtual IntegerArray::value_type operator*() const
    {
        return IntegerArray::value_type(*mItr);
    }

    virtual void operator++()
    {
        ++mItr;
    }

    LazyIteratorBasic(const string& pBaseName, FileFactory& pFactory)
        : mItr(pBaseName, pFactory)
    {
    }

private:
    typename MappedArray<store_type>::LazyIterator mItr;
};


template<typename UprType, typename LwrType>
class LazyIteratorStacked : public IntegerArray::LazyIterator
{
public:
    virtual bool valid() const
    {
        return mItr.valid();
    }

    virtual IntegerArray::value_type operator*() const
    {
        return *mItr;
    }

    virtual void operator++()
    {
        ++mItr;
    }

    LazyIteratorStacked(const string& pBaseName, FileFactory& pFactory)
        : mItr(pBaseName, pFactory)
    {
    }

private:
    typename StackedArray<UprType,LwrType>::LazyIterator mItr;
};


IntegerArray::BuilderPtr
IntegerArray::builder(uint64_t pBits, const string& pBaseName, FileFactory& pFactory)
{
    typedef StackedArray<uint8_t,uint16_t>  Array24;
    typedef StackedArray<uint16_t,uint32_t> Array48;
    typedef StackedArray<uint16_t,uint64_t> Array80;
    typedef StackedArray<uint32_t,uint64_t> Array96;

    typedef IntegerArrayBasicBuilder<uint8_t>               IntegerArrayBuilder8;
    typedef IntegerArrayBasicBuilder<uint16_t>              IntegerArrayBuilder16;
    typedef IntegerArrayStackedBuilder<uint8_t,uint16_t>    IntegerArrayBuilder24;
    typedef IntegerArrayBasicBuilder<uint32_t>              IntegerArrayBuilder32;
    typedef IntegerArrayStackedBuilder<uint8_t,uint32_t>    IntegerArrayBuilder40;
    typedef IntegerArrayStackedBuilder<uint16_t,uint32_t>   IntegerArrayBuilder48;
    typedef IntegerArrayStackedBuilder<uint8_t,Array48>     IntegerArrayBuilder56;
    typedef IntegerArrayBasicBuilder<uint64_t>              IntegerArrayBuilder64;
    typedef IntegerArrayStackedBuilder<uint8_t,uint64_t>    IntegerArrayBuilder72;
    typedef IntegerArrayStackedBuilder<uint16_t,uint64_t>   IntegerArrayBuilder80;
    typedef IntegerArrayStackedBuilder<uint8_t,Array80>     IntegerArrayBuilder88;
    typedef IntegerArrayStackedBuilder<uint32_t,uint64_t>   IntegerArrayBuilder96;
    typedef IntegerArrayStackedBuilder<uint8_t,Array96>     IntegerArrayBuilder104;
    typedef IntegerArrayStackedBuilder<uint16_t,Array96>    IntegerArrayBuilder112;
    typedef IntegerArrayStackedBuilder<Array24,Array96>     IntegerArrayBuilder120;
    typedef IntegerArrayStackedBuilder<uint64_t,uint64_t>   IntegerArrayBuilder128;
    switch (pBits)
    {
        case 8:
        {
            return BuilderPtr(new IntegerArrayBuilder8(pBaseName, pFactory));
        }
        case 16:
        {
            return BuilderPtr(new IntegerArrayBuilder16(pBaseName, pFactory));
        }
        case 24:
        {
            return BuilderPtr(new IntegerArrayBuilder24(pBaseName, pFactory));
        }
        case 32:
        {
            return BuilderPtr(new IntegerArrayBuilder32(pBaseName, pFactory));
        }
        case 40:
        {
            return BuilderPtr(new IntegerArrayBuilder40(pBaseName, pFactory));
        }
        case 48:
        {
            return BuilderPtr(new IntegerArrayBuilder48(pBaseName, pFactory));
        }
        case 56:
        {
            return BuilderPtr(new IntegerArrayBuilder56(pBaseName, pFactory));
        }
        case 64:
        {
            return BuilderPtr(new IntegerArrayBuilder64(pBaseName, pFactory));
        }
        case 72:
        {
            return BuilderPtr(new IntegerArrayBuilder72(pBaseName, pFactory));
        }
        case 80:
        {
            return BuilderPtr(new IntegerArrayBuilder80(pBaseName, pFactory));
        }
        case 88:
        {
            return BuilderPtr(new IntegerArrayBuilder88(pBaseName, pFactory));
        }
        case 96:
        {
            return BuilderPtr(new IntegerArrayBuilder96(pBaseName, pFactory));
        }
        case 104:
        {
            return BuilderPtr(new IntegerArrayBuilder104(pBaseName, pFactory));
        }
        case 112:
        {
            return BuilderPtr(new IntegerArrayBuilder112(pBaseName, pFactory));
        }
        case 120:
        {
            return BuilderPtr(new IntegerArrayBuilder120(pBaseName, pFactory));
        }
        case 128:
        {
            return BuilderPtr(new IntegerArrayBuilder128(pBaseName, pFactory));
        }
        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("IntegerArray::builder: unsupported integer width "
                                                        + lexical_cast<string>(pBits)));
        }
    }
}


IntegerArrayPtr
IntegerArray::create(uint64_t pBits, const string& pBaseName, FileFactory& pFactory)
{
    typedef StackedArray<uint8_t,uint16_t>  Array24;
    typedef StackedArray<uint16_t,uint32_t> Array48;
    typedef StackedArray<uint16_t,uint64_t> Array80;
    typedef StackedArray<uint32_t,uint64_t> Array96;

    typedef IntegerArrayBasic<uint8_t>               IntegerArray8;
    typedef IntegerArrayBasic<uint16_t>              IntegerArray16;
    typedef IntegerArrayStacked<uint8_t,uint16_t>    IntegerArray24;
    typedef IntegerArrayBasic<uint32_t>              IntegerArray32;
    typedef IntegerArrayStacked<uint8_t,uint32_t>    IntegerArray40;
    typedef IntegerArrayStacked<uint16_t,uint32_t>   IntegerArray48;
    typedef IntegerArrayStacked<uint8_t,Array48>     IntegerArray56;
    typedef IntegerArrayBasic<uint64_t>              IntegerArray64;
    typedef IntegerArrayStacked<uint8_t,uint64_t>    IntegerArray72;
    typedef IntegerArrayStacked<uint16_t,uint64_t>   IntegerArray80;
    typedef IntegerArrayStacked<uint8_t,Array80>     IntegerArray88;
    typedef IntegerArrayStacked<uint32_t,uint64_t>   IntegerArray96;
    typedef IntegerArrayStacked<uint8_t,Array96>     IntegerArray104;
    typedef IntegerArrayStacked<uint16_t,Array96>    IntegerArray112;
    typedef IntegerArrayStacked<Array24,Array96>     IntegerArray120;
    typedef IntegerArrayStacked<uint64_t,uint64_t>   IntegerArray128;
    switch (pBits)
    {
        case 8:
        {
            return IntegerArrayPtr(new IntegerArray8(pBaseName, pFactory));
        }
        case 16:
        {
            return IntegerArrayPtr(new IntegerArray16(pBaseName, pFactory));
        }
        case 24:
        {
            return IntegerArrayPtr(new IntegerArray24(pBaseName, pFactory));
        }
        case 32:
        {
            return IntegerArrayPtr(new IntegerArray32(pBaseName, pFactory));
        }
        case 40:
        {
            return IntegerArrayPtr(new IntegerArray40(pBaseName, pFactory));
        }
        case 48:
        {
            return IntegerArrayPtr(new IntegerArray48(pBaseName, pFactory));
        }
        case 56:
        {
            return IntegerArrayPtr(new IntegerArray56(pBaseName, pFactory));
        }
        case 64:
        {
            return IntegerArrayPtr(new IntegerArray64(pBaseName, pFactory));
        }
        case 72:
        {
            return IntegerArrayPtr(new IntegerArray72(pBaseName, pFactory));
        }
        case 80:
        {
            return IntegerArrayPtr(new IntegerArray80(pBaseName, pFactory));
        }
        case 88:
        {
            return IntegerArrayPtr(new IntegerArray88(pBaseName, pFactory));
        }
        case 96:
        {
            return IntegerArrayPtr(new IntegerArray96(pBaseName, pFactory));
        }
        case 104:
        {
            return IntegerArrayPtr(new IntegerArray104(pBaseName, pFactory));
        }
        case 112:
        {
            return IntegerArrayPtr(new IntegerArray112(pBaseName, pFactory));
        }
        case 120:
        {
            return IntegerArrayPtr(new IntegerArray120(pBaseName, pFactory));
        }
        case 128:
        {
            return IntegerArrayPtr(new IntegerArray128(pBaseName, pFactory));
        }
        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("IntegerArray::create: unsupported integer width "
                                                        + lexical_cast<string>(pBits)));
        }
    }
}

IntegerArray::LazyIteratorPtr
IntegerArray::lazyIterator(uint64_t pBits, const string& pBaseName, FileFactory& pFactory)
{
    typedef StackedArray<uint8_t,uint16_t>  Array24;
    typedef StackedArray<uint16_t,uint32_t> Array48;
    typedef StackedArray<uint16_t,uint64_t> Array80;
    typedef StackedArray<uint32_t,uint64_t> Array96;

    typedef LazyIteratorBasic<uint8_t>               LazyIterator8;
    typedef LazyIteratorBasic<uint16_t>              LazyIterator16;
    typedef LazyIteratorStacked<uint8_t,uint16_t>    LazyIterator24;
    typedef LazyIteratorBasic<uint32_t>              LazyIterator32;
    typedef LazyIteratorStacked<uint8_t,uint32_t>    LazyIterator40;
    typedef LazyIteratorStacked<uint16_t,uint32_t>   LazyIterator48;
    typedef LazyIteratorStacked<uint8_t,Array48>     LazyIterator56;
    typedef LazyIteratorBasic<uint64_t>              LazyIterator64;
    typedef LazyIteratorStacked<uint8_t,uint64_t>    LazyIterator72;
    typedef LazyIteratorStacked<uint16_t,uint64_t>   LazyIterator80;
    typedef LazyIteratorStacked<uint8_t,Array80>     LazyIterator88;
    typedef LazyIteratorStacked<uint32_t,uint64_t>   LazyIterator96;
    typedef LazyIteratorStacked<uint8_t,Array96>     LazyIterator104;
    typedef LazyIteratorStacked<uint16_t,Array96>    LazyIterator112;
    typedef LazyIteratorStacked<Array24,Array96>     LazyIterator120;
    typedef LazyIteratorStacked<uint64_t,uint64_t>   LazyIterator128;
    switch (pBits)
    {
        case 8:
        {
            return LazyIteratorPtr(new LazyIterator8(pBaseName, pFactory));
        }
        case 16:
        {
            return LazyIteratorPtr(new LazyIterator16(pBaseName, pFactory));
        }
        case 24:
        {
            return LazyIteratorPtr(new LazyIterator24(pBaseName, pFactory));
        }
        case 32:
        {
            return LazyIteratorPtr(new LazyIterator32(pBaseName, pFactory));
        }
        case 40:
        {
            return LazyIteratorPtr(new LazyIterator40(pBaseName, pFactory));
        }
        case 48:
        {
            return LazyIteratorPtr(new LazyIterator48(pBaseName, pFactory));
        }
        case 56:
        {
            return LazyIteratorPtr(new LazyIterator56(pBaseName, pFactory));
        }
        case 64:
        {
            return LazyIteratorPtr(new LazyIterator64(pBaseName, pFactory));
        }
        case 72:
        {
            return LazyIteratorPtr(new LazyIterator72(pBaseName, pFactory));
        }
        case 80:
        {
            return LazyIteratorPtr(new LazyIterator80(pBaseName, pFactory));
        }
        case 88:
        {
            return LazyIteratorPtr(new LazyIterator88(pBaseName, pFactory));
        }
        case 96:
        {
            return LazyIteratorPtr(new LazyIterator96(pBaseName, pFactory));
        }
        case 104:
        {
            return LazyIteratorPtr(new LazyIterator104(pBaseName, pFactory));
        }
        case 112:
        {
            return LazyIteratorPtr(new LazyIterator112(pBaseName, pFactory));
        }
        case 120:
        {
            return LazyIteratorPtr(new LazyIterator96(pBaseName, pFactory));
        }
        case 128:
        {
            return LazyIteratorPtr(new LazyIterator128(pBaseName, pFactory));
        }
        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("LazyIterator::create: unsupported integer width "
                                                        + lexical_cast<string>(pBits)));
        }
    }
}

void
IntegerArray::remove(const std::string& pBaseName, FileFactory& pFactory)
{
    tryRemove(pBaseName, pFactory, 8);
}

uint64_t
IntegerArray::roundUpBits(uint64_t pBits)
{
    
    if (pBits <= 8)
    {
        return 8;
    }
    if (pBits <= 16)
    {
        return 16;
    }
    if (pBits <= 24)
    {
        return 24;
    }
    if (pBits <= 32)
    {
        return 32;
    }
    if (pBits <= 40)
    {
        return 40;
    }
    if (pBits <= 48)
    {
        return 48;
    }
    if (pBits <= 56)
    {
        return 56;
    }
    if (pBits <= 64)
    {
        return 64;
    }
    if (pBits <= 72)
    {
        return 72;
    }
    if (pBits <= 80)
    {
        return 80;
    }
    if (pBits <= 88)
    {
        return 88;
    }
    if (pBits <= 96)
    {
        return 96;
    }
    if (pBits <= 104)
    {
        return 104;
    }
    if (pBits <= 112)
    {
        return 112;
    }
    if (pBits <= 120)
    {
        return 120;
    }
    if (pBits <= 128)
    {
        return 128;
    }
    BOOST_THROW_EXCEPTION(
        Gossamer::error()
            << Gossamer::general_error_info("IntegerArray::roundUpBits: unsupported integer width "
                                                + lexical_cast<string>(pBits)));
}
