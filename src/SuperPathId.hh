#ifndef SUPERPATHID_HH
#define SUPERPATHID_HH

#ifndef TAGGGEDNUM_HH
#include "TaggedNum.hh"
#endif

struct SuperPathIdTag {};
typedef TaggedNum<SuperPathIdTag> SuperPathId;

namespace boost
{
    template <>
    struct hash<SuperPathId>
    {
        uint64_t operator()(const SuperPathId& pId) const
        {
            return hash<uint64_t>()(pId.value());
        }
    };
}

#endif // SUPERPATHID_HH
