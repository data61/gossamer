// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef XENOVERSION_HH
#define XENOVERSION_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

namespace Gossamer
{
    const char* version = "1.0.0";
    uint64_t versionNumber = 0x001001000; // MMMmmmppp
} // namespace Gossamer

#endif // XENOVERSION_HH
