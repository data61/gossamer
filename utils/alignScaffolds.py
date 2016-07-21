import math
import re
import sys
import subprocess

ref = sys.argv[1]
scafs = sys.argv[2]

def fasta(fn):
    i = None
    s = []
    for l in open(fn):
        if l[0] == '>':
            if i:
                yield (i, ''.join(s))
            i = l[1:-1]
            s = []
        else:
            s.append(l[:-1])
    if i:
        yield (i, ''.join(s))

def cigars(f):
    for l in f:
        t = l.split()
        if len(t) == 0:
            continue
        x = t[0].split(':')
        if x[0] != 'cigar':
            continue
        i = t[1]
        i_s = long(t[2])
        i_e = long(t[3])
        i_d = t[4]
        r = t[5]
        r_s = long(t[6])
        r_e = long(t[7])
        r_d = t[8]
        v = (i, i_s, i_e, i_d, r, r_s, r_e, r_d)
        yield v

def group(cs):
    pi = None
    pn = 0
    ps = []
    for c in cs:
        x = c[0]
        m = re.search('^(.*)_([0-9]+)_([0-9]+)$', x)
        i = m.group(1)
        n = long(m.group(2))
        j = long(m.group(3))
        if i != pi:
            if len(ps):
                yield ps
            ps = []
            pi = i
            pn = 0
        if n != pn:
            pn = n
            ps.append([])
        itm = {'qk':i, 'qn':n, 'ns':j, 'qb':c[1], 'qe':c[2], 'qs':c[3], 'tk':c[4], 'tb':c[5], 'te':c[6], 'ts':c[7]}
        ps[-1].append(itm)
    if len(ps):
        yield ps

def carte(vs):
    if len(vs) == 1:
        for v in vs[0]:
            yield (v,)
    else:
        for t in carte(vs[:-1]):
            for v in vs[-1]:
                l = list(t)
                l.append(v)
                yield tuple(l)

def erf(x):
    # save the sign of x
    sign = 1 if x >= 0 else -1
    x = abs(x)

    # constants
    a1 =  0.254829592
    a2 = -0.284496736
    a3 =  1.421413741
    a4 = -1.453152027
    a5 =  1.061405429
    p  =  0.3275911

    # A&S formula 7.1.26
    t = 1.0/(1.0 + p*x)
    y = 1.0 - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t*math.exp(-x*x)
    return sign*y # erf(-x) = -erf(x)

def ascending(v):
    if len(v) < 2:
        return True
    x = v[0]
    for y in v[1:]:
        if y <= x:
            return False
        x = y
    return True

def descending(v):
    if len(v) < 2:
        return True
    x = v[0]
    for y in v[1:]:
        if y >= x:
            return False
        x = y
    return True

def score(t):
    d = None
    v = []
    for p in t:
        if d is not None:
            if p['qs'] != d:
                return -1
        d = p['qs']
        v.append(p['tb'])
    if d == '+':
        if not ascending(v):
            return -2
    else:
        if not descending(v):
            return -3
    s = 1
    x = None
    g = None
    for p in t:
        if g is not None:
            y = -abs((p['tb'] - x)/float(g) - 1)/3.0
            z = erf(y) + 1
            #print p['ts'], p['tb'], p['tb'] - x, g, y, (erf(-abs(y)) + 1)
            #print y, z
            s *= z
        x = p['te']
        g = p['ns']
    #print
    return s

f = open('pieces.fa', 'w')
for (i,s) in fasta(scafs):
    m = re.search('^([^Nn]+)([Nn]{10}[Nn]*)(.*)$', s)
    pieces = []
    n = 1
    while m:
        print >> f, '>' + i.split()[0] + '_' + str(n) + '_' + str(len(m.group(2)))
        print >> f, m.group(1)
        s = m.group(3)	
        m = re.search('^([^Nn]+)([Nn]{10}[Nn]*)(.*)$', s)
        n += 1
    print >> f, '>' + i.split()[0] + '_' + str(n) + '_0'
    print >> f, s
f.close()

p = subprocess.Popen(["ssaha2", "-output", "cigar", "-identity", "99", ref, "pieces.fa"], stdout=subprocess.PIPE)

for v in group(cigars(p.stdout)):
    lst = []
    for itm in carte(v):
        x = score(itm)
        s = str(x) + '\t'
        q = None
        g = None
        lst.append((-x, itm))
    lst.sort()
    if len(lst) < 1:
        continue
    for p in lst[0][1]:
        if q is not None:
            s += '(' + str(p['tb'] - q) + '/' + str(g) + ') '
        s += p['qs'] + '<' + str(p['tb']) + ',' + str(p['te']) + '> '
        q = p['te']
        g = p['ns']
    s += p['qk']
    print s
