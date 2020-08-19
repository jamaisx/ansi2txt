BINPATH = /usr/bin
MANPATH = /usr/share/man/man1

srcdir = .
CC = gcc
CFLAGS = -O2
LDFLAGS =
LIBS = 


OBJ = ansi2txt.o
SRC = $(srcdir)/ansi2txt.c

ansi2txt: $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

clean:
	rm -f ansi2txt $(OBJ)

install:
	strip ansi2txt
	cp ansi2txt $(BINPATH)
	if test ! -e $(BINPATH)/ansi2txt; then ln -s $(BINPATH)/ansi2txt $(BINPATH)/ansi2html; fi
	cp ansi2txt.1.gz $(MANPATH)

ansi2txt.o: ./ansi2txt.c

