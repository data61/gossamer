// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef MAPPEDARRAY_HH
#define MAPPEDARRAY_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

#ifndef ERRNO_H
#include <errno.h>
#define ERRNO_H
#endif

#ifndef FCNTL_H
#include <fcntl.h>
#define FCNTL_H
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

/*
#ifndef SYS_MMAN_H
#include <sys/mman.h>
#define SYS_MMAN_H
#endif

#ifndef SYS_STAT_H
#include <sys/stat.h>
#define SYS_STAT_H
#endif

#ifndef SYS_TYPES_H
#include <sys/types.h>
#define SYS_TYPES_H
#endif
*/

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

template <typename T, bool BoundsCheck = false >
class MappedArray
{
public:
    class Builder
    {
    public:
        void push_back(const T& pItem)
        {
            mFile.write(reinterpret_cast<const char*>(&pItem), sizeof(T));
        }

        void end()
        {
        }

        Builder(const std::string& pBaseName, FileFactory& pFactory)
            : mFileHolder(pFactory.out(pBaseName)),
              mFile(**mFileHolder),
			  mBaseName(pBaseName)
        {
        }

    private:
        FileFactory::OutHolderPtr mFileHolder;
        std::ostream& mFile;
		std::string mBaseName;
    };

    class Iterator
    {
    public:
        
        bool valid() const
        {
            return mPos < size();
        }

        void operator++()
        {
            mPos++;
        }

        const T& operator*() const
        {
            return (*mArray)[mPos];
        }

        uint64_t position() const
        {
            return mPos;
        }

        uint64_t size() const
        {
            return mArray->size();
        }

        Iterator(const MappedArray<T>* pArray)
            : mArray(pArray), mPos(0)
        {
        }

    private:
        
        const MappedArray<T>* mArray;
        uint64_t mPos;
    };

    class LazyIterator
    {
    public:
        
        bool valid() const
        {
            return mIn.good();
        }

        const T& operator*() const
        {
            return *mBuffer;
        }

        void operator++()
        {
            next();
            ++mPos;
        }

        /**
         * The position of the iterator in the array, in terms of T elements.
         */
        uint64_t position() const
        {
            return mPos;
        }

        /**
         * The size, in T elements, of the array.
         */
        uint64_t size() const
        {
            return mSize;
        }

        LazyIterator(const std::string& pBaseName, FileFactory& pFactory)
            : mInHolder(pFactory.in(pBaseName)), mIn(**mInHolder),
              mBuffer(reinterpret_cast<T*>(malloc(sizeof(T)))), mPos(0)
        {
            uint64_t z = pFactory.size(pBaseName);
            if (z % sizeof(T))
            {
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << boost::errinfo_file_name(pBaseName));
            }
            mSize = z / sizeof(T);
            next();
        }

    private:
        
        void next()
        {
            if (valid())
            {
                mIn.read(reinterpret_cast<char*>(mBuffer.get()), sizeof(T));
            }
        }

        FileFactory::InHolderPtr mInHolder;
        std::istream& mIn;
        uint64_t mSize;
        boost::shared_ptr<T> mBuffer;
        uint64_t mPos;
    };

    Iterator iterator() const
    {
        return Iterator(this);
    }

    static LazyIterator lazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    {
        return LazyIterator(pBaseName, pFactory);
    }

    uint64_t size() const
    {
        return mSize;
    }

    const T& operator[](uint64_t pIdx) const
    {
        if (BoundsCheck)
        {
            BOOST_ASSERT(pIdx < mSize);
        }
        return mItems[pIdx];
    }

    const T* begin() const
    {
        return mItems;
    }

    const T* end() const
    {
        return mItems + mSize;
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putProp("size", size());
        t.putProp("storage", sizeof(T) * size());
        return t;
    }

    MappedArray(const std::string& pBaseName, FileFactory& pFactory)
        : mItemsHolder(pFactory.map(pBaseName)),
          mSize(mItemsHolder->size() / sizeof(T)),
          mItems(reinterpret_cast<const T*>(mItemsHolder->data()))
    {
    }

private:
    FileFactory::MappedHolderPtr mItemsHolder;
    const uint64_t mSize;
    const T* mItems;
};

#endif // MAPPEDARRAY_HH
