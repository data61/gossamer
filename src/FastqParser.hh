// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef FASTQPARSER_HH
#define FASTQPARSER_HH

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

#ifndef LINESOURCE_HH
#include "LineSource.hh"
#endif

#ifndef GOSSREADPARSER_HH
#include "GossReadParser.hh"
#endif

#ifndef GOSSFASTQREADBASESTRING_HH
#include "GossFastqReadBaseString.hh"
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif


// The FASTQ format is documented in:
//
//    Cock et al., "The Sanger FASTQ file format for sequences with
//        quality scores, and the Solexa/Illumina FASTQ variants".
//
//    doi:10.1093/nar/gkp1137
//    http://nar.oxfordjournals.org/content/38/6/1767
//


class FastqParser : public GossReadParser
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

    static void getLine( const LineSource& pSrc, std::string& pDst )
    {
        const std::string& line = *pSrc;
        // for windows where new line is \r\n
        const unsigned int l = line.length();
        if( l > 0 && line.at( l - 1 ) == '\r' )
        {
            pDst = line.substr( 0, l - 1 );
        }
        else
        {
            pDst = line;
        }
    }


    void next()
    {
        if (!mSrc.valid())
        {
            mValid = false;
            return;
        }

        std::string line;
        getLine( mSrc, line );

        if (!(line.size() > 0 && line[0] == '@'))
        {
            mValid = false;
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << boost::errinfo_file_name(in().filename())
                    << Gossamer::parse_error_info("expected '@' at beginning of line " + 
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
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << boost::errinfo_file_name(in().filename())
                        << Gossamer::parse_error_info("expected sequence data or quality header at line " + 
                                                  boost::lexical_cast<std::string>(mLineNum)));

            }
            getLine( mSrc, line );
            if (line.size() > 0 && (line[0] == '@' || line[0] == '+'))
            {
                break;
            }
            mSequence += line;
        }

        if (!(line.size() > 0 && line[0] == '+'))
        {
            mValid = false;
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << boost::errinfo_file_name(in().filename())
                    << Gossamer::parse_error_info("expected '+' at beginning of line " + 
                                                  boost::lexical_cast<std::string>(mLineNum)));
        }

        mQLabel = std::string(line.begin() + 1, line.end());
        if (mQLabel.size() > 0 && mQLabel != mLabel)
        { 
            mValid = false;
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << boost::errinfo_file_name(in().filename())
                    << Gossamer::parse_error_info("quality title does not match sequence title at line " +
                                                  boost::lexical_cast<std::string>(mLineNum)));
        } 

        mQual.clear();
        while (true)
        {
            ++mSrc;
            ++mLineNum;
            if (!mSrc.valid())
            {
                break;
            }
            getLine( mSrc, line );
            if (line.size() > 0 && (line[0] == '@' || line[0] == '+'))
            {
                // The '@' symbol may be present in Sanger-format
                // quality scores.  So if we've seen less quality
                // data than we've seen sequence data, assume this
                // is more quality data.

                if (mQual.size() >= mSequence.size())
                {
                    break;
                }
            }
            mQual += line;
        }

        if (mSequence.size() != mQual.size())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << boost::errinfo_file_name(in().filename())
                    << Gossamer::parse_error_info("length mistmatch between sequence and quality data just before line " +
                                                  boost::lexical_cast<std::string>(mLineNum)));
        }
    }
    
    // NOTE: next() must be called before the first read is available.
    explicit FastqParser(const LineSourcePtr& pLineSrcPtr)
        : GossReadParser(pLineSrcPtr->in()),
          mSrcPtr(pLineSrcPtr), mSrc(*mSrcPtr),
          mValid(true), mLineNum(1), mLabel(), mSequence(), mQLabel(), mQual(),
          mRead(mLabel, mSequence, mQLabel, mQual)
    {
    }

    static GossReadParserPtr
    create(const LineSourcePtr& pLineSrcPtr)
    {
        return std::make_shared<FastqParser>(pLineSrcPtr);
    }

private:
    LineSourcePtr mSrcPtr;
    LineSource& mSrc;
    bool mValid;
    uint64_t mLineNum;
    std::string mLabel;
    std::string mSequence;
    std::string mQLabel;
    std::string mQual;
    GossFastqReadBaseString mRead;
};

#endif // FASTQPARSER_HH
