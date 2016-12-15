// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef FASTAPARSER_HH
#define FASTAPARSER_HH

#ifndef GOSSREADPARSER_HH
#include "GossReadParser.hh"
#endif

#ifndef LINESOURCE_HH
#include "LineSource.hh"
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

#ifndef GOSSFASTAREADBASESTRING_HH
#include "GossFastaReadBaseString.hh"
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

class FastaParser : public GossReadParser
{
public:
    bool valid() const
    {
        return mValid;
    }

    const GossRead& read() const
    {
        return mRead;
    }

    void next()
    {
        if (!mSrc.valid())
        {
            mValid = false;
            return;
        }
        
        const std::string& line(*mSrc);
        if (!(line.size() > 0 && line[0] == '>'))
        {
            mValid = false;
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << boost::errinfo_file_name(in().filename())
                    << Gossamer::parse_error_info("expected '>' at beginning of line " + 
                                                  boost::lexical_cast<std::string>(mLineNum)));
        }
        
        mLabel = std::string(line.begin() + 1, line.end());
        mSequence.clear();
        while (true)
        {
            ++mSrc;
            ++mLineNum;
            if (!mSrc.valid())
            {
                break;
            }
            const std::string& line(*mSrc);
            if (line.size() > 0 && line[0] == '>')
            {
                break;
            }
            mSequence += line;
        }
    }

    // NOTE: next() must be called before the first read is available.
    explicit FastaParser(const LineSourcePtr& pLineSrcPtr)
        : GossReadParser(pLineSrcPtr->in()),
          mSrcPtr(pLineSrcPtr), mSrc(*mSrcPtr),
          mValid(true), mLineNum(0),
          mLabel(), mSequence(), mRead(mLabel, mSequence)
    {
    }

    static GossReadParserPtr
    create(const LineSourcePtr& pLineSrcPtr)
    {
        return std::make_shared<FastaParser>(pLineSrcPtr);
    }

private:
    LineSourcePtr mSrcPtr;
    LineSource& mSrc;
    bool mValid;
    uint64_t mLineNum;
    std::string mLabel;
    std::string mSequence;
    GossFastaReadBaseString mRead;
};

#endif
