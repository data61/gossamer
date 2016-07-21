import math
import pickle
import random
import sys


def getk(l):
    return int(5*math.log(l))

def fixHist(h):
    t = 0
    n = 0
    mxp = 0
    for (p,c) in h.items():
        t += c
        n += 1
        mxp = max(mxp, p)
    t += mxp # pseudo-counts
    t = float(t)
    for i in range(1, mxp + 1):
        if i not in h:
            h[i] = 1.0/t
        else:
            h[i] = (h[i] + 1.0)/t
    p = 0
    for i in range(1, mxp + 1):
        h[i] += p
        p = h[i]
    return (h, mxp)

def genPos(g, u):
    (h, n) = g
    b = 1
    e = n
    m = (b + e) / 2
    while e > b:
        #print b, m, e, u, h[m]
        if h[m] < u:
            b = m + 1
        elif h[m] > u:
            e = m - 1
        else:
            break
        m = (b + e) / 2
    #print u, h[m]
    return m

def mutate(s, p):
    r = []
    for i in range(0, len(s)):
        if random.random() < p:
            r.append(random.choice(['A','C', 'G', 'T']))
        else:
            r.append(s[i])
    return ''.join(r)

def genReads(gs, nm, s, n, L):
    l = len(s)
    k = getk(l)
    g = gs[k]
    (h, xp) = g
    for i in range(0, n):
        p = genPos(g, random.random())
        p = int((len(s) - L) * float(p) / xp)
        print '>' + nm + ' ' + str(p)
        print mutate(s[p:p+L], 0.01)

gs = pickle.load(open('hists.txt'))
for k in gs.keys():
    print k
    gs[k] = fixHist(gs[k])

ss = {}
s = []
nm = ''
for l in open(sys.argv[1]):
    if l[0] == '>':
        if len(s):
            ss[nm] = ''.join(s)
        nm = l[1:].split()[0]
        s = []
        continue
    s.append(l[:-1])
if len(s):
    ss[nm] = ''.join(s)
inv = {}
for (nm, s) in ss.items():
    k = getk(len(s))
    if k not in inv:
        inv[k] = []
    inv[k].append(nm)

random.seed(int(sys.argv[3]))

for l in open(sys.argv[2]):
    t = l.split()
    oldNm = t[0]
    k = getk(int(t[1]))
    c = int(t[2])
    newNm = random.choice(inv[k])
    genReads(gs, newNm, ss[newNm], c, 100)
