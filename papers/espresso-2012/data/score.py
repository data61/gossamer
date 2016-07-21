import math
import sys

e = 0.01

def logFac(n):
	if n == 0:
		return 0.0
        r = n * math.log(n) - n + math.log(n * (1 + 4 * n * (1 + 2 * n))) / 6 + math.log(math.pi) / 2
        return r

def logChoose(n, k):
        return logFac(n) - logFac(k) - logFac(n - k)

def logAdd(a, b):
        x1 = max(a, b)
        x2 = min(a, b)
        w = int(x1)
        y1 = x1 - w
        y2 = x2 - w
        z = math.exp(y1) + math.exp(y2)
        return w + math.log(z)

def logBinEq(p, n, k):
        return logChoose(n, k) + math.log(p) * k + math.log(1 - p) * (n - k)

def logBinGe(p, n, k):
	vs = [logBinEq(p, n, j) for j in range(k, n + 1)]
	r = vs[0]
	for v in vs[1:]:
		r = logAdd(r, v)
        return r

print logBinEq(0.01, 200, 0)
print logBinEq(0.01, 200, 1)
print logBinEq(0.01, 200, 2)
print logBinEq(0.01, 200, 3)
print logBinEq(0.01, 200, 4)
sys.exit(0)

for l in open(sys.argv[1]):
	t = l.split()
	gz = int(t[0])
	hs = [(int(x.split(':')[0]), int(x.split(':')[1])) for x in t[1:]]
	for (gid,h) in hs:
		n = int(math.ceil(float(gz - h)/25.0))
		s = (1.0 - e) ** h * e ** (gz -h)
		ls = math.log10(1.0 - e) * h + math.log10(e) * (gz - h)
		ls1 = logBinEq(e, gz, gz - h) * 0.4342945
		ls2 = logBinGe(e, gz, gz - h) * 0.4342945
		print '%d\t%d\t%d\t%f\t%f' % (gid, gz, (gz - h), ls1, ls2)
		#print '%d\t%d\t%d\t%d\t%d\t%f' % (gid, gz, h, (gz - h), n, ls2)
	print
