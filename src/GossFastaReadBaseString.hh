#ifndef GOSSFASTAREADBASESTRING_HH
#define GOSSFASTAREADBASESTRING_HH

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

#ifndef GOSSREADBASESTRING_HH
#include "GossReadBaseString.hh"
#endif

class GossFastaReadBaseString : public GossReadBaseString
{
public:

    void print(std::ostream& pOut) const
    {
        pOut << '>' << mLabel << std::endl;
        pOut << mRead << std::endl;
    }
    
    GossReadPtr clone() const;

    GossFastaReadBaseString(const std::string& pLabel, const std::string& pRead)
        : GossReadBaseString(pLabel, pRead, mQual), mQual()
    {
    }

private:
    std::string mQual;
};

#endif // GOSSFASTAREADBASESTRING_HH
