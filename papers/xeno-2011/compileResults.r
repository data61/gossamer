library(ellipse)

g = c("ABCG2", "ALDH1A1", "CD177", "DLL1", "DLL3", "GLI1", "GLI2", "HES1",
      "JAG1", "LGR5", "NANOGNB", "NOTCH1", "NOTCH2", "NOTCH3",
      "PTCH1", "PTCH2", "SMO", "JAG2")

h = read.table('bm18-26-results.txt', header=TRUE)
h$tophat_human = h$human_ambiguous + h$human_both + h$human_human + h$human_mouse + h$human_neither
h$xenome_human = h$both_human + h$human_human + h$mouse_human + h$neither_human
h$tophat_both = h$both_ambiguous + h$both_both + h$both_human + h$both_mouse + h$both_neither
h$xenome_both = h$both_both + h$human_both + h$mouse_both + h$neither_both

tt = sum(h$tophat_human)
tx = sum(h$xenome_human)

h$tophat_fpkm = 1e9 * h$tophat_human / h$exLen / tt
h$xenome_fpkm = 1e9 * h$xenome_human / h$exLen / tx

h1 = h[match(g,h$name),]

write.table(h1, 'bm18-26-verify.txt')

j = read.table('bm18-52-results.txt', header=TRUE)
j$tophat_human = j$human_ambiguous + j$human_both + j$human_human + j$human_mouse + j$human_neither
j$xenome_human = j$both_human + j$human_human + j$mouse_human + j$neither_human
j$tophat_both = j$both_ambiguous + j$both_both + j$both_human + j$both_mouse + j$both_neither
j$xenome_both = j$both_both + j$human_both + j$mouse_both + j$neither_both

tt = sum(j$tophat_human)
tx = sum(j$xenome_human)

j$tophat_fpkm = 1e9 * j$tophat_human / j$exLen / tt
j$xenome_fpkm = 1e9 * j$xenome_human / j$exLen / tx

g = c("ABCG2", "ALDH1A1", "CD177", "DLL1", "DLL3", "GLI1", "GLI2", "HES1",
      "JAG1", "LGR5", "NANOGNB", "NOTCH1", "NOTCH2", "NOTCH3",
      "PTCH1", "PTCH2", "SMO", "JAG2")

j1 = j[match(g,j$name),]

write.table(j1, 'bm18-52-verify.txt')

p = read.table("PCR.txt", header=TRUE)

p1 = p[match(g, p$Name),]

p1$diff1 = p1$actin1 - p1$Ct1
p1$diff2 = p1$actin2 - p1$Ct2
p1$diff3 = p1$actin3 - p1$Ct3
p1$diff4 = p1$actin4 - p1$Ct4

pdf('validation.pdf')
plot(c(), c(), xlim=c(log10(min(h1$xenome_fpkm, j1$xenome_fpkm)), log10(max(h1$xenome_fpkm, j1$xenome_fpkm))),
               ylim=c(min(p1$diff1, p1$diff2, p1$diff3, p1$diff4), max(p1$diff1, p1$diff2, p1$diff3, p1$diff4)),
               xlab='log10 fpkm', ylab='Ct(actin) - Ct(gene)',
               type='l')
for (x in g) {
    print(x)
    s = log10(c(h1[h1$name==x,][,c('xenome_fpkm')], j1[j1$name==x,][,c('xenome_fpkm')]))
    q = c(p1[p1$Name==x,][,c('diff1')], p1[p1$Name==x,][,c('diff2')], p1[p1$Name==x,][,c('diff3')], p1[p1$Name==x,][,c('diff4')])
    sm = mean(s)
    qm = mean(q)
    ss = var(s)
    qs = var(q)
    lines(ellipse(matrix(c(ss, 0, 0, qs), ncol=2, nrow=2), centre=c(sm,qm)))
    text(sm, qm, x, cex=0.75)
}
dev.off()
