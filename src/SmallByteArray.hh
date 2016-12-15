// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SMALLBYTEARRAY_HH
#define SMALLBYTEARRAY_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif


class SmallByteArrayBase
{
protected:
    static const size_t sStorageAmount = 8;

    SmallByteArrayBase()
        : mBegin(&mStorage.mStorageLocal[0]),
          mEnd(&mStorage.mStorageLocal[0])
    {
    }
  
    uint8_t* mBegin;
    uint8_t* mEnd;

    union {
        uint8_t* mStorageRemote;
        uint8_t mStorageLocal[sStorageAmount];
    } mStorage;

};


class SmallByteArray : private SmallByteArrayBase
{
public:
    template<typename It>
    SmallByteArray(It pBegin, It pEnd)
    {
        init(pBegin, pEnd);
    }

    SmallByteArray()
    {
	setSizeInternal(0);
    }

    explicit SmallByteArray(size_t pSize)
    {
        setSizeInternal(pSize);
    }

    SmallByteArray(const SmallByteArray& pRhs)
        : SmallByteArrayBase()
    {
        init(pRhs.mBegin, pRhs.mEnd);
    }

    void
    setSize(size_t pSize)
    {
	clean();
	setSizeInternal(pSize);
    }

    uint8_t operator[](size_t pIndex) const
    {
        return mBegin[pIndex];
    }

    uint8_t& operator[](size_t pIndex)
    {
        return mBegin[pIndex];
    }

    size_t size() const
    {
        return mEnd - mBegin;
    }

    uint8_t* begin()
    {
        return mBegin;
    }

    uint8_t* end()
    {
        return mEnd;
    }

    const uint8_t* begin() const
    {
        return mBegin;
    }

    const uint8_t* end() const
    {
        return mEnd;
    }

    SmallByteArray&
    operator=(const SmallByteArray& pRhs)
    {
	if (this != &pRhs)
	{
	    clean();
	    init(pRhs.begin(), pRhs.end());
	}
	return *this;
    }

    ~SmallByteArray()
    {
	clean();
    }

private:
    void clean()
    {
        if (mBegin != &mStorage.mStorageLocal[0])
        {
	    delete[] mStorage.mStorageRemote;
	    mStorage.mStorageRemote = 0;
	    mBegin = mEnd = &mStorage.mStorageLocal[0];
        }
    }

    void
    setSizeInternal(size_t pSize)
    {
        if (pSize <= sStorageAmount)
        {
            mBegin = &mStorage.mStorageLocal[0];
        }
        else
        {
            mStorage.mStorageRemote = new uint8_t[pSize];
            mBegin = mStorage.mStorageRemote;
        }
        mEnd = mBegin + pSize;
    }

    template<typename It>
    void
    init(It pBegin, It pEnd)
    {
        setSize(pEnd - pBegin);
	std::copy(pBegin, pEnd, mBegin);
    }
};


#endif // SMALLBYTEARRAY_HH
