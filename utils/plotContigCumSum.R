#!/usr/bin/R

# Utilities for plotting cumulative sums of vectors.
# This is useful for example in plotting the cumulative sums of the lengths 
# of contigs from an assembler as well as for plotting the actual contig
# lengths in decreasing order.
#
#--------------------------------------------------------------------------
# MANUAL
#
# How to plot several cumulative sums of contig lengths in decreasing order
# and the corresponding contig lengths as a function of rank.
# Start R
#--------------------------------------------------------------------------
# Now source this R file.
#--------------------------------------------------------------------------
# > source("/pathTo/plotContigCumSum.R")

#--------------------------------------------------------------------------
# Now define a data frame with the values to be plotted. The data frame has 
# column names which will be used as abbreviations. The first row contains 
# the filenames and the second row contains the names of the column to be 
# plotted. The files contain tab separated columns with a descriptive column
# name in the header row.
# Note that the name of the column can vary. Eg in the following columns
# with name Length and width are used, depending on the file.
# Here is an example:
#--------------------------------------------------------------------------
# > ns=data.frame(seg=c("segments.txt", "Length"), seg2=c("segments2.txt", "Length"), seg3=c("segments3.txt", "width"))

#--------------------------------------------------------------------------
# To produce a ggplot object for the cumulative sums and write it to 
# a pdf file use the following.
#--------------------------------------------------------------------------
# > p=ggPlotCumSum(ns)
# seg segments.txt 
# seg2 segments2.txt 
# seg3 segments3.txt 
# > pdf("cumsum.pdf")
# > print(p)
# > dev.off()
# X11cairo 
       # 2 

#--------------------------------------------------------------------------
# To produce a ggplot object for the contig lengths in decreasing order
# and write it to a pdf file use the following.
#--------------------------------------------------------------------------
# > p=ggPlotLengths(ns)
# seg segments.txt 
# seg2 segments2.txt 
# seg3 segments3.txt 
# > pdf("lengths.pdf")
# > print(p)
# > dev.off()
# X11cairo 
       # 2 

#--------------------------------------------------------------------------
# If you want to just plot it to a window that can be done too:
# following are some examples.
#--------------------------------------------------------------------------
# > print(p)
# > print(p + scale_y_log10())
# > print(p + scale_y_log10()+xlim=c(1,200))
# > print(p + scale_y_log10()+xlim(1,200))


library(ggplot2)

# Get the n50 value from a reverse sorted cumulative sum vector.
getN50 <- function(d)
{
    n50x=min(which(d>=median(d)))
    n50=dfPE$contigLength[n50x]
    n50
}

# Given a vector calculate its reverse cumulative sum and
# return in a data frame with rank, contig length and cumulative sum columns.
getCumSum <- function(v)
{
    s=rev(sort(v))
    df=data.frame(rank=1:length(s), contigLength=s, cumsum=cumsum(s))
    df
}

# From a data frame of shortNames with filenames, do the plotting.
# Read each file, extract the length column, and build a data frame of the
# cumsum using rbind.
# Input:
#      namesdf: a data frame with column names, first row of filenames 
#          and second row of column name in the file.
#          Example: ns=data.frame(seg=c("segments.txt", "Length"), seg2=c("segments2.txt", "Length"), seg3=c("segments3.txt", "width"))
# Output:
#     results: dataframe with columns Type, rank, contigLength and cumsum.
getCumSums <- function(namesdf)
{
    first = FALSE
    results = NA
    for (n in names(namesdf))
    {
        cat(paste(n, namesdf[1,n], sep=" "), "\n")
        
        # Read the tab separated file.
        data = read.table(toString(namesdf[1,n]), header=TRUE, fill=TRUE, sep="\t")
        #info = getCumSum(data$Length)
        info = getCumSum(data[, toString(namesdf[2,n])])
        
        if (first) {
            results = data.frame(Type=n, value=info)
        } else {
            results <-rbind(results, data.frame(Type=n, value=info))
        }
    }
    results
}

# ggplot the cumulative sums of a set of files.
# Input:
#      namesdf: a data frame with column names, first row of filenames 
#          and second row of column name in the file.
#          Example: ns=data.frame(seg=c("segments.txt", "Length"), seg2=c("segments2.txt", "Length"), seg3=c("segments3.txt", "width"))
#      minContigLength: the minimum contig length for plotting.
#      xlabel: A title for the x axis.
#      ylabel: A title for the y axis.
# Output:
#     p: a ggplot object which can be used for subsequent plotting.
ggPlotCumSum <- function(namesdf, minContigLength=100, xlabel="Rank of reverse sorted contig length", ylabel="Cumulative sum of contig lengths")
{
    results = getCumSums(namesdf)

    p=ggplot(subset(results,subset=results$value.contigLength >=100), aes(value.rank,value.cumsum,colour=Type)) + xlab(xlabel) +  ylab(ylabel) + geom_line()

}

# ggplot the contig lengths of a set of files.
# Input:
#      namesdf: a data frame with column names, first row of filenames 
#          and second row of column name in the file.
#          Example: ns=data.frame(seg=c("segments.txt", "Length"), seg2=c("segments2.txt", "Length"), seg3=c("segments3.txt", "width"))
#      minContigLength: the minimum contig length for plotting.
#      xlabel: A title for the x axis.
#      ylabel: A title for the y axis.
# Output:
#     p: a ggplot object which can be used for subsequent plotting.
ggPlotLengths <- function(namesdf, minContigLength=100, xlabel="Rank of reverse sorted contig length", ylabel="Contig lengths")
{
    results = getCumSums(namesdf)

    p=ggplot(subset(results,subset=results$value.contigLength >=100), aes(value.rank,value.contigLength,colour=Type)) + xlab(xlabel) +  ylab("Contig Length") + geom_line()

    p
}


