import math
import pickle
import random
import sys

lens = {}
llens = {}
res = {}
rhists = {}

for l in sys.stdin:
    if len(l) == 0:
        continue
    t = l.split()
    if t[0] == '@SQ':
        lens[t[1][3:]] = int(t[2][3:])
        llens[t[1][3:]] = int(5*math.log(int(t[2][3:])))
    if t[0][0] == '@':
        continue
    if t[2] == '*':
        continue
    if t[2] not in res:
        res[t[2]] = 0
    res[t[2]] += 1
    k = llens[t[2]]
    if k not in rhists:
        rhists[k] = {}
    p = int(t[3])
    if p not in rhists[k]:
        rhists[k][p] = 0
    rhists[k][p] += 1

#inv = {}
#for (nm,k) in llens.items():
#    if k not in inv:
#        inv[k] = []
#    inv[k].append(nm)
#
#for (k,nms) in inv.items():
#    random.shuffle(nms)

for (g,c) in res.items():
    k = llens[g]
    #ng = inv[k].pop()
    print g + '\t' + str(lens[g]) + '\t' + str(c)
    #print g + '\t' + str(lens[g]) + '\t' + ng + '\t' + str(lens[ng]) + '\t' + str(c)
    #print g + '\t' + str(c) + '\t' + str(5*int(math.log(c))) + '\t' + str(lens[g]) + '\t' + str(int(5*math.log(lens[g])))


f = open('hists.txt', 'w')
pickle.dump(rhists, f)
f.close()
