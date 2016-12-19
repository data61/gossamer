if (CMAKE_HOST_APPLE)
    # OS X is a Unix, but it's not a normal Unix as far as search paths go.
    set(GOSS_SEARCH_PATHS
        /usr
        /Library/Frameworks
        /Applications/Xcode.app/Contents/Developer/Platforms.MacOSX.platform/Developer/SDKs
        /usr/local
        /opt/local
    )
elseif (CMAKE_HOST_UNIX)
    set(GOSS_SEARCH_PATHS
        /usr
        /usr/local
        /opt/local
    )
elseif (CMAKE_HOST_WIN32)
    message(WARNING "This build has not yet been tested on Win32")
    set(GOSS_SEARCH_PATHS
        /usr
        /usr/local
        /opt/local
    )
else ()
    message(WARNING "This build has not yet been tested on this platform")
    set(GOSS_SEARCH_PATHS)
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(PLATFORM_CXX_FLAGS
        "-std=c++11 -stdlib=libc++ -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-float-equal"
	)
    set(PLATFORM_LINKER_FLAGS "-stdlib=libc++")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	if(MY_GLIBCXX_HAS_THE_WRONG_ABI)
            set(PLATFORM_GLIBCXX_ABI_FLAG "-D_GLIBCXX_USE_CXX11_ABI=0")
        else(MY_GLIBCXX_HAS_THE_WRONG_ABI)
            set(PLATFORM_GLIBCXX_ABI_FLAG "-D_GLIBCXX_USE_CXX11_ABI=1")
        endif(MY_GLIBCXX_HAS_THE_WRONG_ABI)
	set(PLATFORM_CXX_FLAGS "-Winline -Wall -fomit-frame-pointer -ffast-math -std=c++11 ${PLATFORM_GLIBCXX_ABI_FLAG}")
    set(PLATFORM_LINKER_FLAGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    message(WARNING "This build has not yet been tested with Intel compiler")
    set(PLATFORM_CXX_FLAGS)
    set(PLATFORM_LINKER_FLAGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    message(WARNING "This build has not yet been tested with MSVVC++")
    set(PLATFORM_CXX_FLAGS)
    set(PLATFORM_LINKER_FLAGS)
else()
    message(WARNING "This build may not have been ported to this compiler")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKER_FLAGS}")


