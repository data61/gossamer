// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef INTEGERARRAY_HH
#define INTEGERARRAY_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

#ifndef BIGINTEGER_HH
#include "BigInteger.hh"
#endif

#ifndef RANKSELECT_HH
#include "RankSelect.hh"
#endif

class IntegerArray;
typedef boost::shared_ptr<IntegerArray> IntegerArrayPtr;

class IntegerArray
{
public:
    typedef Gossamer::position_type::value_type value_type;

    class Builder
    {
    public:
        typedef IntegerArray::value_type value_type;
        virtual void push_back(value_type pItem) = 0;

        virtual void end() = 0;

        virtual ~Builder() {}
    };
    typedef boost::shared_ptr<Builder> BuilderPtr;

    class LazyIterator
    {
    public:
        /**
         * Return true iff the iterator may be dereferenced.
         */
        virtual bool valid() const = 0;

        /**
         * Dereference the iterator to get the current value.
         */
        virtual value_type operator*() const = 0;

        /**
         * Advance the iterator to the next value.
         */
        virtual void operator++() = 0;

        virtual ~LazyIterator()
        {
        }
    };
    typedef boost::shared_ptr<LazyIterator> LazyIteratorPtr;

    /**
     * Return the number of elements in the integer array.
     */
    virtual uint64_t size() const = 0;

    /**
     * Retrieve an element of the integer array.
     */
    virtual value_type operator[](uint64_t pIdx) const = 0;

    /**
     * Find the first position in the interval [pBegin, pEnd) containing a value >= pValue.
     */
    virtual uint64_t lower_bound(uint64_t pBegin, uint64_t pEnd, const value_type& pValue) const = 0;

    /**
     * Find the first position in the interval [pBegin, pEnd) containing a value > pValue.
     */
    virtual uint64_t upper_bound(uint64_t pBegin, uint64_t pEnd, const value_type& pValue) const = 0;

    /**
     * Find out some information about the IntegerArray.
     */
    virtual PropertyTree stat() const = 0;

    /**
     * Construct a new IntegerArray. The number of bits per item (pBits) must be valid. Valid numbers
     * of bits may be generated with roundUpBits() below.
     */
    static BuilderPtr builder(uint64_t pBits, const std::string& pBaseName, FileFactory& pFactory);

    /**
     * Open an existing IntegerArray. The number of bits per item (pBits) must be valid. Valid numbers
     * of bits may be generated with roundUpBits() below.
     */
    static IntegerArrayPtr create(uint64_t pBits, const std::string& pBaseName, FileFactory& pFactory);

    /**
     *
     */
    static LazyIteratorPtr lazyIterator(uint64_t pBits, const std::string& pBaseName, FileFactory& pFactory);

    /**
     * Remove the files for the named integer array.
     */
    static void remove(const std::string& pBaseName, FileFactory& pFactory);

    /**
     * Round up pBits to the nearest valid number of bits storable in an integer array.
     */
    static uint64_t roundUpBits(uint64_t pBits);

    virtual ~IntegerArray() {}
};

#endif // INTEGERARRAY_HH
