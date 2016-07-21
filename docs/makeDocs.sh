pandoc -s -w man gossamerManpage.txt -o goss.1
pandoc -s -w context gossamerManpage.txt -o goss.tex
texexec --pdf goss.tex

pandoc -s -w man gosspleManpage.txt -o gossple.1
pandoc -s -w context gosspleManpage.txt -o gossple.tex
texexec --pdf gossple.tex

pandoc -s -w man xenomeManpage.txt -o xenome.1
pandoc -s -w context xenomeManpage.txt -o xenome.tex
texexec --pdf xenome.tex

pandoc -s -w man electusManpage.txt -o electus.1
pandoc -s -w context electusManpage.txt -o electus.tex
texexec --pdf electus.tex
