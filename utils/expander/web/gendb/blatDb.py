import os
import sqlite3
import subprocess
import sys
import bisect

def usage():
    print('usage: ' + sys.argv[0] + ' <contig_db> <ref> [ref_gene]')

if len(sys.argv) < 3:
    usage()
    exit(1)

dbFile = sys.argv[1];
refFile = sys.argv[2];
geneRefFile = sys.argv[3] if len(sys.argv) >= 4 else ''
tmpFastaFile = "contigs.fa"
tmpPslFile = "aligns.psl"

if os.path.isfile(tmpFastaFile):
    print('Cannot write temporary FASTA output to ' + tmpFastaFile + '. File already exists.')
    exit(1)

if os.path.isfile(tmpPslFile):
    print('Cannot write temporary PSL output to ' + tmpPslFile + '. File already exists.')
    exit(1)

conn = sqlite3.connect(dbFile)
cur = conn.cursor()

# Dump contigs
print('dumping contigs ... ')
out = open(tmpFastaFile, 'w')
for row in cur.execute('SELECT * FROM sequences'):
    id, seq = row
    if len(seq) > 0:
        print >>out, '>' + str(id)
        print >>out, seq
out.close()

genes = []

# Parse gene ref
if len(geneRefFile) > 0:
    for l in open(geneRefFile):
        t = l.split('\t')
        genes.append((t[2], long(t[4]), long(t[5]), t[12]))
genes.sort()

def overlap(a0, b0, a1, b1):
    if a1 < a0:
        if b1 < a0:
            return False
        else:
            return True
    elif a1 > b0:
        return False;
    else:
        return True

def geneId(ref, dir, start, end):
    dummy = (ref, start, end, '')
    i = bisect.bisect_left(genes, dummy)
    if i:
        if overlap(start, end, genes[i][1], genes[i][2]):
            return genes[i][3]
        elif i < len(genes) - 1 and overlap(start, end, genes[i+1][1], genes[i+1][2]):
            return genes[i+1][3]
    return ''


# Invoke BLAT
print('invoking aligner ... ')
subprocess.call(['blat', refFile, tmpFastaFile, tmpPslFile])

aligns = {}

# Parse PSL
print('parsing alignments ... ')
for l in open(tmpPslFile):
    t = l.split()
    if (len(t) == 21 and t[0].isdigit()):
        match = t[0]
        dir = 1 if t[8] == '+' else -1
        query = t[9]
        target = t[13]
        start = t[15]
        end = t[16]
        if (query not in aligns) or (long(aligns[query][4]) < long(match)):
            aligns[query] = (query, target, start, end, match, dir)
 
# Write alignment info.
print('writing alignments ... ')
cur.execute('DROP TABLE alignments')
cur.execute('CREATE TABLE alignments (id INTEGER PRIMARY KEY ASC, name TEXT, start INTEGER, end INTEGER, matchLen INTEGER, dir INTEGER, gene TEXT)')
cur.execute('CREATE INDEX index_gene ON alignments (gene)')
cur.execute('CREATE INDEX index_name ON alignments (name)')
cur.execute('CREATE INDEX index_start ON alignments (start)')
cur.execute('CREATE INDEX index_end ON alignments (end)')
conn.commit()
for a in aligns:
    align = aligns[a]
    gene = '' if len(genes) == 0 else geneId(align[1], align[5], long(align[2]), long(align[3]))
    rec = align + (gene,)
    # print(rec)
    cur.execute('INSERT INTO alignments VALUES (?, ?, ?, ?, ?, ?, ?)', rec)
conn.commit()

# Clean up
print('cleaning up ... ')
os.unlink(tmpFastaFile)
os.unlink(tmpPslFile)
