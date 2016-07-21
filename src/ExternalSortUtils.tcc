
template<typename Lhs, typename Rhs, typename Dest>
void
ExternalSortBase::merge(Lhs& pLhs, Rhs& pRhs, Dest& pDest)
{
    while (pLhs.valid() && pRhs.valid())
    {
        if (*pLhs < *pRhs)
        {
            pDest.push_back(*pLhs);
            ++pLhs;
            continue;
        }
        else
        {
            pDest.push_back(*pRhs);
            ++pRhs;
            continue;
        }
    }
    copy(pLhs, pDest);
    copy(pRhs, pDest);
}


