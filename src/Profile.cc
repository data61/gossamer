// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Profile.hh"

std::mutex Profile::sMutex;
boost::thread_specific_ptr<Profile::NodePtr> Profile::sCurrentNodePtr;
Profile::RootsPtr Profile::sThreadRoots;
