#include <cctype>
#include <fstream>
#include <ios>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <tr1/unordered_map>

#include <endian.h>
#include <stdint.h>
#include <unistd.h>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace std::tr1;
using namespace boost;

namespace {
 
    // Reference, direction ("+"/"-"), start, end, gene
    typedef tuple<string, string, uint64_t, uint64_t, string> Ref;
    typedef vector<Ref> Refs;

    // Matched bases, ref name, direction ("+"/"-"), target start, target end
    typedef tuple<uint64_t, string, string, uint64_t, uint64_t> Alignment;  

    // Other contig, gap, count
    typedef tuple<uint64_t, int64_t, uint64_t> Link;
    typedef vector<Link> Links;
    
    // Mean coverage
    typedef tuple<double> Coverage;

    const string magic("GENDB101");

    void usage(char* prog)
    {
        cout << "usage: " << prog << " -c <contigs> -l <links> -g <graph_prefix> [-a <alignment_psl>] [-r <refgene>] -o <output> [-h]\n";
        cout << "(output format: " << magic << ")\n";
    }

    void write(ostream& out, const uint64_t& x)
    {
        uint64_t y = htobe64(x);
        out.write((char*)&y, 8);
    }

    void write(ostream& out, const double& x)
    {
        uint64_t y = *reinterpret_cast<const uint64_t*>(&x);
        write(out, y);
    }

    void write(ostream& out, const string& x)
    {
        out.write(x.c_str(), x.size());
    }

    void write(ostream& out, const Link& x)
    {
        write(out, x.get<0>());
        write(out, *reinterpret_cast<const uint64_t*>(&x.get<1>()));
        write(out, x.get<2>());
    }

    void write(ostream& out, const Alignment& x)
    {
        write(out, x.get<1>().size());
        write(out, x.get<1>());
        write(out, x.get<3>());
        write(out, x.get<4>());
        write(out, x.get<0>());
        write(out, x.get<2>());
    }

    void write(ostream& out, const Coverage& x)
    {
        write(out, x.get<0>());
    }

    char reverseComplement(char b)
    {
        switch (b)
        {
            case 'A': return 'T';
            case 'C': return 'G';
            case 'G': return 'C';
            case 'T': return 'A';
            case 'a': return 't';
            case 'c': return 'g';
            case 'g': return 'c';
            case 't': return 'a';
            default: return b;
        }
    }

    string reverseComplement(const string& seq)
    {
        string rc;
        rc.reserve(seq.size());
        for (string::const_reverse_iterator i = seq.rbegin(); i != seq.rend(); ++i)
        {
            rc.push_back(reverseComplement(*i));
        }
        return rc;
    }

    uint64_t headerId(const string& header)
    {
        // Skip '>', then parse first integer.
        string id;
        for (uint64_t i = 1; i < header.size(); ++i)
        {
            if (!isdigit(header[i]))
            {
                if (id.empty())
                {
                    continue;
                }
                break;
            }
            else
            {
                id.push_back(header[i]);
            }
        }
        if (!id.empty())
        {
            return lexical_cast<uint64_t>(id);
        }
        return 0;
    }

    Coverage headerCoverage(const string& header)
    {
        vector<string> bits;
        split(bits, header, is_any_of(","));
        double mean = 0.0;
        if (bits.size() >= 6)
        {
            mean = lexical_cast<double>(bits[5]);
        }
        return Coverage(mean);
    }

    bool overlap(uint64_t a0, uint64_t b0, uint64_t a1, uint64_t b1)
    {
        if (a1 < a0)
        {
            if (b1 < a0)
            {
                return false;
            }
            return true;
        }
        else if (a1 > b0)
        {
            return false;
        }
        return true;
    }

    bool match(const Alignment& align, const Ref& ref)
    {
        if (   align.get<1>() != ref.get<0>()
            || align.get<2>() != ref.get<1>())
        {
            return false;
        }

        return overlap(align.get<3>(), align.get<4>(), ref.get<2>(), ref.get<3>());
    }

    string geneId(const Alignment& align, const Refs& refs)
    {
        static const string none("");
        Ref dummy(align.get<1>(), align.get<2>(), align.get<3>(), align.get<4>(), "");
        Refs::const_iterator itr = lower_bound(refs.begin(), refs.end(), dummy);
        if (itr == refs.end())
        {
            return none;
        }

        // Alignment can only match the pointed-to gene, or its immediate predecessor.
        if (match(align, *itr))
        {
            return (*itr).get<4>();
        }

        if (itr == refs.begin())
        {
            return none;
        }

        --itr;
        if (match(align, *itr))
        {
            return (*itr).get<4>();
        }

        return none;
    }

};


int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        usage(argv[0]);
        return 1;
    }

    string contigFile;
    string linkFile;
    string rcFile;
    string pslFile;
    string refFile;
    string outFile;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "a:c:g:hl:o:r:")) != -1)
    {
        switch (c)
        {
            case 'a':
                pslFile = optarg;
                break;

            case 'c':
                contigFile = optarg;
                break;

            case 'g':
                rcFile = string(optarg) + "-supergraph.rcs.rc-path-ids";
                break;
                
            case 'h':
                usage(argv[0]);
                return 0;

            case 'l':
                linkFile = optarg;
                break;

            case 'o':
                outFile = optarg;
                break;

            case 'r':
                refFile = optarg;
                break;

            case '?':
                cerr << "unrecognised option -" << (char)optopt << '\n';
                return 1;
        }
    }

    if (contigFile.empty())
    {
        cerr << "no contig file!\n";
        return 1;
    }
    
    if (linkFile.empty())
    {
        cerr << "no link file!\n";
        return 1;
    }
    
    if (rcFile.empty())
    {
        cerr << "no graph prefix!\n";
        return 1;
    }

    if (optind < argc)
    {
        cerr << "unrecognised option " << argv[optind] << '\n';
        return 1;
    }

    unordered_map<uint64_t, string> contigs;
    unordered_map<uint64_t, uint64_t> lengths;
    map<uint64_t, uint64_t> rcs;
    unordered_map<uint64_t, Links> fwdLinks;
    unordered_map<uint64_t, Links> bckLinks;
    unordered_map<uint64_t, Alignment> alignments;
    unordered_map<uint64_t, Coverage> coverage;
    Refs refs;

    // Read input.

    // refgene
    {
        cerr << "loading gene reference ... ";
        uint64_t numLines = 0;
        string line;
        ifstream in(refFile.c_str());
        if (in.good())
        {
            getline(in, line);
            while (line.size())
            {
                ++numLines;
                vector<string> bits;
                split(bits, line, is_any_of("\t"));

                const string& ref = bits[2];
                // const string& dir = bits[3];
                const uint64_t start = lexical_cast<uint64_t>(bits[4]);
                const uint64_t end = lexical_cast<uint64_t>(bits[5]);
                const string& gene = bits[12];
                // refs.push_back(Ref(ref, dir, start, end, gene));
                // Ignore strand.
                refs.push_back(Ref(ref, "-", start, end, gene));
                refs.push_back(Ref(ref, "+", start, end, gene));
                getline(in, line);
            }
            sort(refs.begin(), refs.end());
        }
        cerr << numLines << '\n';
    }

    // rcs
    {
        cerr << "loading rcs ... ";
        ifstream in(rcFile.c_str(), ios_base::binary);
        for (uint64_t i = 0; ; ++i)
        {
            int64_t j;
            in.read((char*)(&j), 8);
            if (!in.good())
            {
                break;
            }
            rcs.insert(make_pair(i, j));
        }
        cerr << rcs.size() << '\n';
    }

    // links
    {
        cerr << "loading links ... ";
        uint64_t numLinks = 0;
        ifstream in(linkFile.c_str());
        while (in.good())
        {
            while (in.good())
            {
                char k;
                in.get(k);
                if (!isspace(k))
                {
                    in.unget();
                    break;
                }
            }
            if (!in.good())
            {
                break;
            }

            uint64_t l;
            uint64_t r;
            uint64_t c;
            int64_t g;
            in >> l >> r >> c >> g;
            if (in.fail() || in.bad())
            {
                throw "failed to read from scaffold file!";
            }

            numLinks += 1;
            fwdLinks[l].push_back(Link(r, g, c));
            bckLinks[r].push_back(Link(l, g, c));
        }
        cerr << numLinks << '\n';
    }

    // contigs
    {
        cerr << "loading contigs ... ";
        stringstream ss;
        ifstream in(contigFile.c_str());
        if (in.good())
        {
            string line;
            getline(in, line);
            while (line.size() && line[0] == '>')
            {
                ss.str("");
                const uint64_t id = headerId(line);
                const uint64_t rc = rcs[id];
                Coverage cov = headerCoverage(line);
                getline(in, line);
                while (line.size() && line[0] != '>')
                {
                    ss << line;
                    getline(in, line);
                }

                string seq = ss.str();
                contigs[id] = seq;
                contigs[rc] = reverseComplement(seq);
                lengths[id] = seq.size();
                lengths[rc] = seq.size();
                coverage[id] = cov;
                coverage[rc] = cov;
            }
        }
        cerr << contigs.size() << '\n';
    }

    // alignments
    {
        cerr << "loading alignments ... ";
        uint64_t numAligns = 0;
        ifstream in(pslFile.c_str());
        if (in.good())
        {
            string line;
            getline(in, line);
            // Consume lines until we get to the first one with a leading digit (the first `match' value).
            while (!in.eof() && (line.empty() || !isdigit(line[0])))
            {
                getline(in, line);
            }
            while (line.size())
            {
                vector<string> bits;
                split(bits, line, is_any_of("\t"));

                const uint64_t match = lexical_cast<uint64_t>(bits[0]);
                const uint64_t query = lexical_cast<uint64_t>(bits[9]);
                const string& dir = bits[8];
                const string& target = bits[13];
                const uint64_t start = lexical_cast<uint64_t>(bits[15]);
                const uint64_t end = lexical_cast<uint64_t>(bits[16]);
                
                unordered_map<uint64_t, Alignment>::const_iterator itr(alignments.find(query));
                Alignment align(match, target, dir, start, end);
                if (itr == alignments.end() || itr->second.get<0>() < match)
                {
                    alignments[query] = align;
                    ++numAligns;
                }

                getline(in, line);
            }
        }
        cerr << numAligns << '\n';
    }


    // Validate input.
    // TODO!


    // Produce output.
    const uint64_t numEntries(rcs.size());
    ofstream out(outFile.c_str(), ios_base::binary);
    out.write(magic.c_str(), magic.size());
    write(out, numEntries);
    streampos tocPos = out.tellp();

    // Seek past the TOC and write each entry.
    map<uint64_t, uint64_t> offsets;
    out.seekp(numEntries * 8, ios_base::cur);

    // Format of each entry:
    //  rc:             uint64_t
    //  length:	        uint64_t
    //  numFwdLinks:    uint64_t
    //  numBckLinks:    uint64_t
    //  fwdLinks:       numFwdLinks * sizeof(link)
    //  bckLinks:       numBckLinks * sizeof(link)
    //  sequence:	length
    //  alignment:      align
    //  coverage:       cover
    //
    // where a link is:
    //  otherId:        uint64_t
    //  gap:            int64_t
    //  count:          uint64_t
    //
    // and an align is:
    //  nameLen         uint64_t
    //  name            nameLen
    //  start           uint64_t
    //  end             uint64_t
    //  matchLen        uint64_t
    //  dir             uint8_t
    //  geneLen         uint64_t
    //  geneName        geneLen
    // 
    // and a coverage is:
    //  mean            double

    cerr << "writing entries ... \n";
    Links noLinks;
    for (map<uint64_t, uint64_t>::const_iterator i = rcs.begin(); i != rcs.end(); ++i)
    {
        const uint64_t id = i->first;
        offsets[id] = out.tellp();
        write(out, i->second);
        write(out, lengths[id]);

        unordered_map<uint64_t, Links>::const_iterator fwdItr(fwdLinks.find(id));
        unordered_map<uint64_t, Links>::const_iterator bckItr(bckLinks.find(id));
        const Links& fwds(fwdItr != fwdLinks.end() ? fwdItr->second : noLinks);
        const Links& bcks(bckItr != bckLinks.end() ? bckItr->second : noLinks);
        write(out, uint64_t(fwds.size()));
        write(out, uint64_t(bcks.size()));
        for (Links::const_iterator j = fwds.begin(); j != fwds.end(); ++j)
        {
            write(out, *j);
        }
        for (Links::const_iterator j = bcks.begin(); j != bcks.end(); ++j)
        {
            write(out, *j);
        }
        write(out, contigs[id]);

        Alignment align(0, "", "?", 0, 0);
        unordered_map<uint64_t, Alignment>::const_iterator itrAln = alignments.find(id);
        if (itrAln != alignments.end())
        {
            align = itrAln->second;
        }
        write(out, align);

        string gene = geneId(align, refs);
        write(out, gene.size());
        write(out, gene);
        write(out, coverage[id]);
    }

    // Go back and write the TOC.
    cerr << "writing TOC ... \n";
    out.seekp(tocPos);
    for (map<uint64_t, uint64_t>::const_iterator i = offsets.begin(); i != offsets.end(); ++i)
    {
        write(out, i->second);
    }

    cerr << "done!\n";
    return 0;
}
