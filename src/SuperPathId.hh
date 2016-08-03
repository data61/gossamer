#ifndef SUPERPATHID_HH
#define SUPERPATHID_HH

#ifndef TAGGGEDNUM_HH
#include "TaggedNum.hh"
#endif

struct SuperPathIdTag {};
typedef TaggedNum<SuperPathIdTag> SuperPathId;

namespace std
{
    template <>
    struct hash<SuperPathId>
    {
        uint64_t operator()(const SuperPathId& pId) const
        {
            return std::hash<uint64_t>()(pId.value());
        }
    };
}

#endif // SUPERPATHID_HH
