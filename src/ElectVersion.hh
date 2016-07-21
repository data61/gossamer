#ifndef ELECTVERSION_HH
#define ELECTVERSION_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

namespace Gossamer
{
    const char* version = "1.0.0";
    uint64_t versionNumber = 0x001000000; // MMMmmmppp
} // namespace Gossamer

#endif // ELECTVERSION_HH
