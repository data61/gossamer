// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef HEAP_HH
#define HEAP_HH

#include <vector>

#include <assert.h>

template <typename T>
struct HeapTraits
{
    bool operator()(const T& pLhs, const T& pRhs) const
    {
        return pLhs < pRhs;
    }
};

template <typename T,typename C = HeapTraits<T> >
class Heap : public std::vector<T>
{
public:
    // Verify the heap condition
    void verify()
    {
        for (unsigned long i = 2; i <= (*this).size(); ++i)
        {
            assert(C()((*this)[i / 2 - 1], (*this)[i - 1]));
        }
    }

    // Turn the vector of items into a heap.
    void heapify();

    // Signal that the item on the front of the
    // heap has been modified, and reassert
    // the heap condition.
    void front_changed();

    // Remove the front item off the heap.
    void pop();

private:

    // Move up the heap ensuring the heap invariant is maintained.
    void upheap(unsigned long pK);

    // Move down the heap ensuring the heap invariant is maintained.
    void downheap(unsigned long pK);

};

#include "Heap.tcc"

#endif // HEAP_HH
