// (c) National ICT Australia 2008 (All Rights Reserved)
// This source code is the intellectual property of National ICT Australia
// (NICTA), and may not be redistributed without express permission.


// Turn the vector of items into a heap.
//
template <typename T,typename C>
void
Heap<T,C>::heapify()
{
    for (uint64_t i = 0; i < (*this).size(); ++i)
    {
        upheap(i);
    }
}


// Signal that the item on the front of the
// heap has been modified.
//
template <typename T,typename C>
void
Heap<T,C>::front_changed()
{
    downheap(0);
}


// Remove the front item off the heap.
//
template <typename T,typename C>
void
Heap<T,C>::pop()
{
    (*this)[0] = (*this).back();
    (*this).pop_back();
    if (!(*this).empty())
    {
        downheap(0);
    }
}


// Move up the heap ensuring the heap invariant is maintained.
//
template <typename T,typename C>
void
Heap<T,C>::upheap(unsigned long pK)
{
    unsigned long k = pK + 1;
    unsigned long k2 = k / 2;
    T v = (*this)[k - 1];
    while (k2 >= 1 && C()(v, (*this)[k2 - 1]))
    {
        (*this)[k - 1] = (*this)[k2 - 1];
        k = k2;
        k2 = k / 2;
    }
    (*this)[k - 1] = v;
}


// Move down the heap ensuring the heap invariant is maintained.
//
template <typename T,typename C>
void
Heap<T,C>::downheap(unsigned long pK)
{
    const unsigned long n = this->size();
    unsigned long k = pK + 1;
    T v = (*this)[k - 1];
    while (k <= n / 2)
    {
        unsigned long j = 2 * k;
        if (j < n && C()((*this)[j], (*this)[j - 1]))
        {
            j++;
        }
        if (C()(v, (*this)[j - 1]))
        {
            break;
        }
        (*this)[k - 1] = (*this)[j - 1];
        k = j;
    }
    (*this)[k - 1] = v;
}
