// (C) National ICT Australia 2008 (All Rights Reserved)
// This source code is the intellectual property of National ICT Australia
// (NICTA), and may not be redistributed without express permission.
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
