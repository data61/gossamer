// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef LINESOURCE_HH
#define LINESOURCE_HH

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef BOOST_MAKE_SHARED_HPP
#include <boost/make_shared.hpp>
#define BOOST_MAKE_SHARED_HPP
#endif

#ifndef BOOST_UTILITY_HPP
#include <boost/utility.hpp>
#define BOOST_UTILITY_HPP
#endif

#ifndef BOOST_NONCOPYABLE_HPP
#include <boost/noncopyable.hpp>
#define BOOST_NONCOPYABLE_HPP
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef BACKGROUNDBLOCKPRODUCER_HH
#include "BackgroundBlockProducer.hh"
#endif


class LineSource : private boost::noncopyable
{
private:
    FileThunkIn mIn;

protected:
    LineSource(const FileThunkIn& pIn)
        : mIn(pIn)
    {
    }

public:
    typedef std::string value_type;

    /**
     * Return true if there is at least one line remaining in the source.
     */
    virtual bool valid() const = 0;

    /**
     * Get the current line.
     */
    virtual const std::string& operator*() const = 0;

    /**
     * Advance to the next line.
     */
    virtual void operator++() = 0;

    const FileThunkIn& in() const
    {
        return mIn;
    }

    virtual ~LineSource();
};


typedef boost::shared_ptr<LineSource> LineSourcePtr;

class PlainLineSource : public LineSource
{
public:
    bool valid() const;

    const value_type& operator*() const;

    void operator++();

    PlainLineSource(const FileThunkIn& pIn);

    static LineSourcePtr
    create(const FileThunkIn& pIn)
    {
        return boost::make_shared<PlainLineSource>(pIn);
    }

private:
    FileFactory::InHolderPtr mFileHolder;
    std::istream& mFile;
    std::string mLine;
};


class BackgroundLineSource : public LineSource
{
public:
    bool valid() const;

    const value_type& operator*() const;

    void operator++();

    BackgroundLineSource(const FileThunkIn& pIn);

    static LineSourcePtr
    create(const FileThunkIn& pIn)
    {
        return boost::make_shared<BackgroundLineSource>(pIn);
    }

private:
    PlainLineSource mPlainLineSource;
    BackgroundBlockProducer<PlainLineSource> mBackground;
};

typedef std::function<LineSourcePtr (const FileThunkIn&)> LineSourceFactory;

#endif // LINESOURCE_HH
