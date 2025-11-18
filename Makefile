COVER := -V titlepage -V titlepage-skip-title -V titlepage-background=doc/cover.pdf
DOCFLAGS := -V toc -V toc-own-page -V colorlinks
TEXFLAGS := --pdf-engine=xelatex --template=doc/template.tex --listings -H doc/syntax.tex

%.pdf: %.md doc/template.tex doc/syntax.tex Makefile
	pandoc $< -f markdown -t latex -o $@ $(TEXFLAGS) $(COVER) $(DOCFLAGS)

all: README.pdf DESIGN.pdf

.PHONY: format
format:
	find . -not \( -path ./build -prune \) -not \( -path ./ext -prune \) \( -iname \*.h -o -iname \*.c \) -type f -print0 | xargs -I{} -0 clang-format -i {}
