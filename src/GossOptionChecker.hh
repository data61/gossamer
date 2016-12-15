// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSOPTIONCHECKER_HH
#define GOSSOPTIONCHECKER_HH

#ifndef BOOST_PROGRAM_OPTIONS_HH
#include <boost/program_options.hpp>
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

#ifndef APP_HH
#include "App.hh"
#endif

class GossOptionChecker
{
public:
    template <typename T>
    class True
    {
    public:
        bool isUsageCheck() const
        {
            return true;
        }

        bool operator()(const T& pVal) const
        {
            return true;
        }

        std::string failed(const T& pVal) const
        {
            BOOST_ASSERT(false);
            return "";
        }

        True()
        {
        }
    };

    class RangeCheck
    {
    public:
        bool isUsageCheck() const
        {
            return true;
        }

        bool operator()(uint64_t pVal) const
        {
            return pVal <= mMax;
        }

        std::string failed(uint64_t pVal) const
        {
            std::string s("\tvalue " + boost::lexical_cast<std::string>(pVal) + " is out of range.\n"
                          "\tthe maximum allowed value is " + boost::lexical_cast<std::string>(mMax) + ".\n");
            return s;
        }

        RangeCheck(uint64_t pMax)
            : mMax(pMax)
        {
        }
    private:
        const uint64_t mMax;
    };
    
    class FileCreateCheck
    {
    public:
        bool isUsageCheck() const
        {
            return false;
        }

        bool operator()(const std::string& pName) const
        {
            std::string file(pName);
            if (mCheckPrefix)
            {
                file += ".test";
            }
            try
            {
                mFactory.out(file);
                mFactory.remove(file);
            }
            catch (...)
            {
                return false;       
            }
            return true;
        }

        std::string failed(const std::string& pName) const
        {
            return    mCheckPrefix 
                    ? "\tcannot create filenames with prefix '" + pName + "'\n"
                    : "\tcannot create file '" + pName + "'\n";
        }

        FileCreateCheck(FileFactory& pFactory, bool pCheckPrefix)
            : mCheckPrefix(pCheckPrefix), mFactory(pFactory)
        {
        }

    private:
        
        bool mCheckPrefix;
        FileFactory& mFactory;
    };

    class FileReadCheck
    {
    public:
        bool isUsageCheck() const
        {
            return false;
        }

        bool operator()(const std::string& pName) const
        {
            try
            {
                mFactory.in(pName);
            }
            catch (...)
            {
                return false;
            }
            return true;
        }

        std::string failed(const std::string& pName) const
        {
            return "\tcannot open file '" + pName + "' for reading\n";
        }

        FileReadCheck(FileFactory& pFactory)
            : mFactory(pFactory)
        {
        }

    private:

        FileFactory& mFactory;
    };

    template <typename T>
    bool getMandatory(const std::string& pName, T& pVal)
    {
        if (mOpts.count(pName) == 0)
        {
            mErrors += "mandatory option " + pName + " was not given.\n";
            mSuggestUsage = true;
            return false;
        }
        pVal = mOpts[pName].as<T>();
        return true;
    }

    template <typename T, typename P>
    bool getMandatory(const std::string& pName, T& pVal, const P& pPred)
    {
        if (mOpts.count(pName) == 0)
        {
            mErrors += "mandatory option " + pName + " was not given.\n";
            mSuggestUsage = true;
            return false;
        }
        pVal = mOpts[pName].as<T>();
        if (!pPred(pVal))
        {
            mErrors += "The given value of the option " + pName + " was invalid.\n";
            mErrors += pPred.failed(pVal);
            mSuggestUsage |= pPred.isUsageCheck();
            return false;
        }
        return true;
    }

    template <typename T>
    bool getOptional(const std::string& pName, T& pVal)
    {
        if (mOpts.count(pName) == 0)
        {
            return false;
        }
        pVal = mOpts[pName].as<T>();
        return true;
    }

    template <typename T>
    bool getOptional(const std::string& pName, boost::optional<T>& pVal)
    {
        if (mOpts.count(pName) == 0)
        {
            return false;
        }
        pVal = mOpts[pName].as<T>();
        return true;
    }

    template <typename T, typename P>
    bool getOptional(const std::string& pName, T& pVal, const P& pPred)
    {
        if (mOpts.count(pName) == 0)
        {
            return false;
        }
        pVal = mOpts[pName].as<T>();
        if (!pPred(pVal))
        {
            mErrors += "The given value of the option " + pName + " was invalid.\n";
            mErrors += pPred.failed(pVal);
            mSuggestUsage |= pPred.isUsageCheck();
            return false;
        }
        return true;
    }

    template <typename T>
    bool getRepeatingOnce(const std::string& pName, T& pVal)
    {
        if (mOpts.count(pName) == 0)
        {
            mErrors += "mandatory option " + pName + " was not given.\n";
            mSuggestUsage = true;
            return false;
        }
        std::vector<T> vals = mOpts[pName].as<std::vector<T> >();

        if (vals.size() != 1)
        {
            mErrors += "mandatory option " + pName + " must be supplied exactly once.\n";
            mSuggestUsage = true;
            return false;
        }

        pVal = vals[0];
        return true;
    }

    template <typename T>
    bool getRepeatingTwice(const std::string& pName, T& pLhs, T& pRhs)
    {
        if (mOpts.count(pName) == 0)
        {
            mErrors += "mandatory option " + pName + " was not given.\n";
            mSuggestUsage = true;
            return false;
        }
        std::vector<T> vals = mOpts[pName].as<std::vector<T> >();

        if (vals.size() != 2)
        {
            mErrors += "mandatory option " + pName + " must be supplied exactly twice.\n";
            mSuggestUsage = true;
            return false;
        }

        pLhs = vals[0];
        pRhs = vals[1];
        return true;
    }

    template <typename T>
    bool getRepeating0(const std::string& pName, std::vector<T>& pVals)
    {
        if (mOpts.count(pName) == 0)
        {
            return false;
        }
        pVals = mOpts[pName].as<std::vector<T> >();
        return true;
    }

    template <typename T, typename P>
    bool getRepeating0(const std::string& pName, std::vector<T>& pVals, const P& pPred)
    {
        if (mOpts.count(pName) == 0)
        {
            return false;
        }
        pVals = mOpts[pName].as<std::vector<T> >();
        bool ok = true;
        for (uint64_t i = 0; i < pVals.size(); ++i)
        {
            if (!pPred(pVals[i]))
            {
                mErrors += "The given value of the option " + pName + " was invalid.\n";
                mErrors += pPred.failed(pVals[i]);
                mSuggestUsage |= pPred.isUsageCheck();
                ok = false;
            }
        }
        return ok;
    }

    template <typename T>
    bool getRepeating1(const std::string& pName, std::vector<T>& pVals)
    {
        if (mOpts.count(pName) == 0)
        {
            mErrors += "repeating option " + pName + " must be given at least once.\n";
            mSuggestUsage = true;
            return false;
        }
        pVals = mOpts[pName].as<std::vector<T> >();

        if (pVals.size() == 0)
        {
            mErrors += "repeating option " + pName + " must be given at least once.\n";
            mSuggestUsage = true;
            return false;
        }
        return true;
    }

    template <typename T, typename P>
    bool getRepeating1(const std::string& pName, std::vector<T>& pVals, const P& pPred)
    {
        if (!getRepeating1(pName, pVals))
        {
            return false;
        }

        bool ok = true;
        for (uint64_t i = 0; i < pVals.size(); ++i)
        {
            if (!pPred(pVals[i]))
            {
                mErrors += "The given value of the option " + pName + " was invalid.\n";
                mErrors += pPred.failed(pVals[i]);
                mSuggestUsage |= pPred.isUsageCheck();
                ok = false;
            }
        }
        return ok;
    }

    /**
     * Log an error if both named flags are present.
     */
    bool exclusive(const std::string& pName1, const std::string& pName2)
    {
        if (mOpts.count(pName1) && mOpts.count(pName2))
        {
            mErrors += "options " + pName1 + " and " + pName2 + " cannot be used together.\n";
            mSuggestUsage = true;
            return false;
        }
        return true;
    }

    /**
     * Log an error if more than one of the named flags is present.
     */
    bool exclusive(const std::string& pName1, const std::string& pName2,
                   const std::string& pName3, const std::string& pName4)
    {
        std::vector<std::string> used;
        if (mOpts.count(pName1))
        {
            used.push_back(pName1);
        }
        if (mOpts.count(pName2))
        {
            used.push_back(pName2);
        }
        if (mOpts.count(pName3))
        {
            used.push_back(pName3);
        }
        if (mOpts.count(pName4))
        {
            used.push_back(pName4);
        }

        if (used.size() > 1)
        {
            mErrors += "options " + used[0];
            for (uint64_t i = 1; i < used.size(); ++i)
            {
                mErrors += " and " + used[i];
            }
            mErrors += " cannot be used together.\n";
            mSuggestUsage = true;
        }
        return true;
    }

    void expandFilenames(const std::vector<std::string>& pFiles,
                         std::vector<std::string>& pNames,
                         FileFactory& pFactory)
    {
        for (uint64_t i = 0; i < pFiles.size(); ++i)
        {
            FileFactory::InHolderPtr inp = pFactory.in(pFiles[i]);
            std::istream& in(**inp);
            while (in.good())
            {
                std::string fn;
                std::getline(in, fn);
                if (!in.good())
                {
                    break;
                }
                pNames.push_back(fn);
            }
        }

    }

    void addError(const std::string& pError)
    {
        mErrors += pError;
    }

    void throwIfNecessary(App& pApp)
    {
        bool helped = false;
        if (mOpts.count("help"))
        {
            pApp.help(true);
            helped = true;
        }
        if (mErrors.size() > 0)
        {
            Gossamer::error err;
            if (!helped && mSuggestUsage)
            {
                err << Gossamer::usage_info(mErrors);
            }
            else
            {
                err << Gossamer::general_error_info(mErrors);
            }
            BOOST_THROW_EXCEPTION(err);
        }
    }

    GossOptionChecker(const boost::program_options::variables_map& pOpts)
        : mOpts(pOpts), mErrors(), mSuggestUsage(false)
    {
    }

private:
    const boost::program_options::variables_map& mOpts;
    std::string mErrors;
    bool mSuggestUsage;
};

template <>
inline
bool
GossOptionChecker::getOptional<bool>(const std::string& pName, bool& pVal)
{
    pVal = mOpts.count(pName);
    return pVal;
}

#endif // GOSSOPTIONCHECKER_HH
