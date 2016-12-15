// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef LINEPARSER_HH
#define LINEPARSER_HH

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

#ifndef LINESOURCE_HH
#include "LineSource.hh"
#endif

#ifndef GOSSREADPARSER_HH
#include "GossReadParser.hh"
#endif

#ifndef GOSSREADBASESTRING_HH
#include "GossReadBaseString.hh"
#endif

#ifndef STD_FSTREAM
#include <fstream>
#define STD_FSTREAM
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

/**
 * Parse files containing one sequence per line.
 *
 * This class parses files which contain one sequence per line,
 * unlike fasta files which contain a header starting with '>' 
 * between the sequences (which may span multiple lines).
 * Empty lines are treated as valid lines since this format may
 * use interleaving for paired information and an empty line would
 * imply an empty member of the pair.
 */
class LineParser : public GossReadParser
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

        mSequence = *mSrc;
        mQual = std::string(mSequence.size(), 'B');
        ++mSrc;
    }

    // NOTE: next() must be called before the first read is available.
    explicit LineParser(const LineSourcePtr& pLineSrcPtr)
        : GossReadParser(pLineSrcPtr->in()),
          mSrcPtr(pLineSrcPtr), mSrc(*mSrcPtr),
          mValid(true), mSequence(), mRead(mLabel, mSequence, mQual)
    {
    }

    static GossReadParserPtr
    create(const LineSourcePtr& pLineSrcPtr)
    {
        return std::make_shared<LineParser>(pLineSrcPtr);
    }

private:
    LineSourcePtr mSrcPtr;
    LineSource& mSrc;
    bool mValid;
    std::string mLabel;
    std::string mSequence;
    std::string mQual;
    GossReadBaseString mRead;
};

#endif
