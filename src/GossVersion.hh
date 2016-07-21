#ifndef GOSSVERSION_HH
#define GOSSVERSION_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

namespace Gossamer
{
    const char* version = "1.3.0-dev";
    uint64_t versionNumber = 0x001003000; // MMMmmmppp
} // namespace Gossamer

#endif // GOSSVERSION_HH
