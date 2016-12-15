// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Utils.hh"

// disable these stupid windows macros
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef GOSS_COMBINE_TESTS
    #include <boost/test/unit_test.hpp>
    BOOST_AUTO_TEST_SUITE( GOSS_TEST_MODULE )
#else
    // stand alone tests
    #define BOOST_TEST_MODULE GOSS_TEST_MODULE
    #include <boost/test/included/unit_test.hpp>
#endif

MachineAutoSetup machineSetup;
