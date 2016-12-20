if (CMAKE_HOST_APPLE)
    # OS X is a Unix, but it's not a normal Unix as far as search paths go.
    message(STATUS "This is an Apple platform")
    set(PLATFORM_FLAGS "-DGOSS_PLATFORM_OSX")
    set(PLATFORM_LDFLAGS)
    set(GOSS_SEARCH_PATHS
        /usr
        /Library/Frameworks
        /Applications/Xcode.app/Contents/Developer/Platforms.MacOSX.platform/Developer/SDKs
        /usr/local
        /opt/local
    )
elseif (CMAKE_HOST_UNIX)
    message(STATUS "This is a Unix-like platform")
    set(PLATFORM_FLAGS "-DGOSS_PLATFORM_UNIX")
    set(PLATFORM_LDFLAGS "-L/lib64 -L/usr/lib/x86_64-linux-gnu/ -lpthread")
    set(GOSS_SEARCH_PATHS
        /usr
        /usr/local
        /opt/local
    )
elseif (CMAKE_HOST_WIN32)
    message(STATUS "This is a Windows platform")
    set(PLATFORM_FLAGS "-DGOSS_PLATFORM_WINDOWS")
    set(PLATFORM_LDFLAGS)
    message(WARNING "This build has not yet been tested on Windows")
    set(GOSS_SEARCH_PATHS
        /usr
        /usr/local
        /opt/local
    )
else ()
    message(WARNING "This build has not yet been tested on this platform")
    set(PLATFORM_FLAGS)
    set(PLATFORM_LDFLAGS)
    set(GOSS_SEARCH_PATHS)
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    message(STATUS "The compiler is Clang")
    set(COMPILER_VENDOR "-DGOSS_COMPILER_CLANG")
    set(PLATFORM_CXX_FLAGS
	    "-std=c++11 -stdlib=libc++ -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-float-equal"
	)
    set(COMPILER_LDFLAGS "-stdlib=libc++")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message(STATUS "The compiler is GNU")
    set(COMPILER_VENDOR "-DGOSS_COMPILER_GNU")
    if(MY_GLIBCXX_HAS_THE_WRONG_ABI)
        set(PLATFORM_GLIBCXX_ABI_FLAG "-D_GLIBCXX_USE_CXX11_ABI=0")
    else(MY_GLIBCXX_HAS_THE_WRONG_ABI)
        set(PLATFORM_GLIBCXX_ABI_FLAG "-D_GLIBCXX_USE_CXX11_ABI=1")
    endif(MY_GLIBCXX_HAS_THE_WRONG_ABI)
    set(PLATFORM_CXX_FLAGS "-Winline -Wall -fomit-frame-pointer -ffast-math -std=c++11 ${PLATFORM_GLIBCXX_ABI_FLAG}")
    set(COMPILER_LDFLAGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    message(STATUS "The compiler is Intel")
    set(COMPILER_VENDOR "-DGOSS_COMPILER_INTEL")
    message(WARNING "This build has not yet been tested with ICC")
    set(PLATFORM_CXX_FLAGS)
    set(COMPILER_LDFLAGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    message(STATUS "The compiler is Microsoft")
    set(COMPILER_VENDOR "-DGOSS_COMPILER_MSVC")
    message(WARNING "This build has not yet been tested with MSVC")
    set(PLATFORM_CXX_FLAGS)
    set(COMPILER_LDFLAGS)
else()
    message(WARNING "This build may not have been ported to this compiler")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_FLAGS} ${COMPILER_VENDOR} ${PLATFORM_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LDFLAGS} ${COMPILER_LDFLAGS}")

