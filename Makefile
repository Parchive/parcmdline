
CFLAGS=-g -W -Wall -Wno-unused -O2

par: backend.o checkpar.o makepar.o rwpar.o rs.o md5.o fileops.o main.o readoldpar.o interface.o ui_text.o
	$(CC) -o $@ $^

clean:
	rm -f core par par.exe *.o

all: par

par.exe:
	make clean
	make CC="dos-gcc -s"

install: par
	install par ${HOME}/bin/
