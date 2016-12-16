if (CMAKE_HOST_APPLE)
    # OS X is a Unix, but it's not a normal Unix as far as search paths go.
    set(GOSS_SEARCH_PATHS
        /usr
        /Library/Frameworks
        /Applications/Xcode.app/Contents/Developer/Platforms.MacOSX.platform/Developer/SDKs
        /usr/local
        /opt/local
    )
else (CMAKE_HOST_APPLE)
    if (CMAKE_HOST_UNIX)
        set(GOSS_SEARCH_PATHS
            /usr
            /usr/local
            /opt/local
        )
    endif (CMAKE_HOST_UNIX)
endif (CMAKE_HOST_APPLE)

if (CMAKE_HOST_WIN32)
    message(WARNING "This build has not yet been tested on Win32")
    set(GOSS_SEARCH_PATHS
        /usr
        /usr/local
        /opt/local
    )
endif (CMAKE_HOST_WIN32)

if (MSVC)
    message(WARNING "This build has not yet been tested with VC++")
    set(PLATFORM_CXX_FLAGS)
    set(PLATFORM_LINKER_FLAGS)
endif (MSVC)

if (GCC)
    set(PLATFORM_CXX_FLAGS "-Winline -Wall -fomit-frame-pointer -ffast-math -std=c++11")
    set(PLATFORM_LINKER_FLAGS)
endif (GCC)

if (CMAKE_HOST_APPLE)
    set(PLATFORM_CXX_FLAGS
        "-std=c++11 -stdlib=libc++ -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-float-equal"
	)
    set(PLATFORM_LINKER_FLAGS "-stdlib=libc++")
endif (CMAKE_HOST_APPLE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKER_FLAGS}")


