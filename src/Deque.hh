#ifndef DEQUE_HH
#define DEQUE_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

template <typename T>
class Deque
{
public:
    uint64_t size() const
    {
        return mFront.size() + mBack.size();
    }

    void push_back(const T& pItem)
    {
        mBack.push_back(pItem);
    }

    const T& front() const
    {
        flipBackIfNecessary();
        return mFront.back();
    }

    T& front()
    {
        flipBackIfNecessary();
        return mFront.back();
    }

    void pop_front()
    {
        flipBackIfNecessary();
        mFront.pop_back();
    }

    void reserve(uint64_t pSize)
    {
        mBack.reserve(pSize);
    }

private:

    void flipBackIfNecessary() const
    {
        if (mFront.size())
        {
            return;
        }
        mFront.insert(mFront.end(), mBack.rbegin(), mBack.rend());
        mBack.clear();
    }

    mutable std::vector<T> mFront;
    mutable std::vector<T> mBack;
};

#endif // DEQUE_HH
