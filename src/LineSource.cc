// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "LineSource.hh"


LineSource::~LineSource()
{
}


bool
PlainLineSource::valid() const
{
    return mFile.good() || mLine.size();
}


const LineSource::value_type&
PlainLineSource::operator*() const
{
    return mLine;
}


void
PlainLineSource::operator++()
{
    BOOST_ASSERT(valid());
    mLine.clear();
    std::getline(mFile, mLine);
}


PlainLineSource::PlainLineSource(const FileThunkIn& pIn)
    : LineSource(pIn), mFileHolder(pIn()), mFile(**mFileHolder)
{
    if (mFile.good())
    {
        mLine.clear();
        std::getline(mFile, mLine);
    }
}


bool
BackgroundLineSource::valid() const
{
    return mBackground.valid();
}


const LineSource::value_type&
BackgroundLineSource::operator*() const
{
    return *mBackground;
}


void
BackgroundLineSource::operator++()
{
    ++mBackground;
}


BackgroundLineSource::BackgroundLineSource(const FileThunkIn& pIn)
    : LineSource(pIn), mPlainLineSource(pIn),
      mBackground(mPlainLineSource, 128, 128)
{
}


