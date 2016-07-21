
paper.pdf : paper.tex paper.bib \
		pict-spectral.pdf pict-tips.pdf pict-bubbles.pdf \
		pict-contigs.pdf
	pdflatex paper
	bibtex paper
	pdflatex paper
	pdflatex paper

pict-spectral.pdf : pict-spectral.tex
	pdflatex pict-spectral

pict-tips.pdf : pict-tips.tex
	pdflatex pict-tips

pict-bubbles.pdf : pict-bubbles.tex
	pdflatex pict-bubbles

pict-contigs.pdf : pict-contigs.tex
	pdflatex pict-contigs
