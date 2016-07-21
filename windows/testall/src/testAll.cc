
#ifdef GOSS_COMBINE_TESTS
    
    // we need a file to be the main()
    #define BOOST_TEST_MAIN
    #include <boost/test/unit_test.hpp>

#endif
// else we're not using this file.