
paper.pdf : paper.tex paper.bib \
		pict-matrix.pdf
	pdflatex paper
	bibtex paper
	pdflatex paper
	pdflatex paper

pict-matrix.pdf : pict-matrix.tex
	pdflatex pict-matrix
