// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include <errno.h>
#include <fcntl.h>
//#include <sys/mman.h>
//#include <sys/stat.h>
//#include <sys/types.h>
//#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <boost/filesystem.hpp>

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

// Constructor
// NOTE: unfortunately boost memmory mapped files does not allow
// pre-populating the buffer, probably because not all platforms
// support it.
//TODO: use windows native memory mapping to get pre-population of buffer
template <typename T>
void
MappedFile<T>::open(const char* pFileName, bool pPopulate, Permissions pPerms, Mode pMode)
{
    using namespace boost;
    using namespace boost::iostreams;

    mapped_file_params params;
    params.path = pFileName;
    if( pMode == Private ) // private mode only supports read/write
    {
        // this mode is actually read/write but writes
        // will not be flushed to file
        params.flags = mapped_file::mapmode::priv;
    }
    else
    {
        if( pPerms == ReadOnly )
        {
            params.flags = mapped_file::mapmode::readonly;
        }
        else 
        {
            params.flags = mapped_file::mapmode::readwrite;
        }
    }
    // defaults for the rest params to map the whole file

	uint64_t z = 0;
    uintmax_t size = boost::filesystem::file_size(pFileName);
    if(    size != static_cast<uintmax_t>(-1)
		&& size > 0 )
    {
		mMappedFile.open(params);
		if (!mMappedFile.is_open())
		{
			BOOST_THROW_EXCEPTION(
				Gossamer::error()
					<< boost::errinfo_file_name(pFileName)
					<< boost::errinfo_errno(errno));
		}

		z = sizeof(T) * (mMappedFile.size() / sizeof(T));
	}

    if (z == 0)
    {
        mSize = 0;
        mBase = NULL;
        return;
    }

    // Note that in read only mode mapped_file::data() returns null, so
    // we have to use mapped_file::const_data() and the nasty cast.
    mBase = reinterpret_cast<T*>( const_cast<char*>(mMappedFile.const_data()) );
    mSize = z / sizeof(T);
}


// Destructor
//
template <typename T>
MappedFile<T>::~MappedFile()
{
    mMappedFile.close();
}
