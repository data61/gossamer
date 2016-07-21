import sys
import math

revCompDict = { 'a':'t', 'A':'T', 'c':'g', 'C':'G', 'g':'c', 'G':'C', 't':'a', 'T':'A' }

def revComp(x):
    r = ''
    for c in x:
        if c in revCompDict:
            r += revCompDict[c]
        else:
            r += c
    return r[::-1]

def canonical(x):
    y = revComp(x)
    return min(x, y)

def kmers(k, s):
    r = {}
    for i in range(0, len(s) - k + 1):
        x = canonical(s[i: i + k])
        if x not in r:
            r[x] = 0
        r[x] += 1
    return r

def hist(ks):
    h = {}
    for k,f in ks.items():
        if f not in h:
            h[f] = 0
        h[f] += 1
    return h

def total(ks):
    t = 0
    for k,f in ks.items():
        t += f
    return t

def seqs(f):
    nm = None
    ss = []
    for l in f:
        if l[0] == '>':
            if nm is not None:
                yield((nm, ''.join(ss)))
            nm = l[1:-1]
            ss = []
        else:
            ss.append(l[:-1])
    if nm is not None:
        yield ((nm, ''.join(ss)))

for K in range(56, 100):
    st = 0.0
    su = 0.0
    n = 0
    for (nm, seq) in seqs(open(sys.argv[1])):
        n += 1
        if n > 5000:
            break
        ks = kmers(K, seq)
        h = hist(ks)
        u = 0
        if 1 in h:
            u = h[1]
        t = total(ks)
        st += t
        su += u
    print '%d\t%d\t%d\t%f\t%f\t%f' % (K, su, st, su/st, (st - su)/st, math.log10((st - su)/st))
    sys.stdout.flush()
