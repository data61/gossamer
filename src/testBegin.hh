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
