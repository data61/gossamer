// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef TRIVIALVECTOR_HH
#define TRIVIALVECTOR_HH

template <typename T,int N>
class TrivialVector
{
public:
    uint64_t size() const
    {
        return mSize;
    }

    const T& operator[](uint64_t pIdx) const
    {
        return mItems[pIdx];
    }

    T& operator[](uint64_t pIdx)
    {
        return mItems[pIdx];
    }

    const T& back() const
    {
        return mItems[mSize - 1];
    }

    T& back()
    {
        return mItems[mSize - 1];
    }

    const T* begin() const
    {
        return mItems;
    }

    const T* end() const
    {
        return mItems + mSize;
    }

    void push_back(const T& pItem)
    {
        mItems[mSize++] = pItem;
    }

    void clear()
    {
        mSize = 0;
    }

    TrivialVector()
        : mSize(0)
    {
    }

private:
    uint64_t mSize;
    T mItems[N];
};

#endif // TRIVIALVECTOR_HH
