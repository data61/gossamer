// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef TRANSLUCENTVERSION_HH
#define TRANSLUCENTVERSION_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

namespace Gossamer
{
    const char* version = "0.0.8B";
    uint64_t versionNumber = 0x000000008; // MMMmmmppp
} // namespace Gossamer

#endif // TRANSLUCENTVERSION_HH
