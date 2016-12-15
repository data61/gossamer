// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Utils.hh"

#if defined(GOSS_LINUX_X64)
#include "MachDepLinux.cc"
#elif defined(GOSS_MACOSX_X64)
#include "MachDepMacOSX.cc"
#elif defined(GOSS_WINDOWS_X64)
#include "MachDepWindows.cc"
#else
#error "Something went wrong with the machine-dependent port"
#endif


