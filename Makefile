
CFLAGS=-g -W -Wall -Wno-unused -O2

par: checkpar.o makepar.o rwpar.o rs.o md5.o fileops.o main.o readoldpar.o interface.o ui_text.o
	$(CC) -o $@ $^

clean:
	rm -f core par par.exe *.o

all: par

par.exe:
	make clean
	make CC="dos-gcc -s"

install: par
	install par ${HOME}/bin/

main.o: main.c checkpar.h types.h par.h rwpar.h interface.h
checkpar.o: checkpar.c checkpar.h util.h fileops.h types.h par.h
makepar.o: makepar.c makepar.h util.h fileops.h types.h par.h
md5.o: md5.c md5.h types.h
fileops.o: fileops.c fileops.h types.h util.h par.h md5.h
rwpar.o: rwpar.c rwpar.h par.h rs.h util.h md5.h
rs.o: rs.c rs.h types.h fileops.h util.h
readoldpar.o: readoldpar.c readoldpar.h fileops.h util.h par.h
interface.o: interface.c interface.h par.h rwpar.h fileops.h util.h types.h
ui_text.o: ui_text.c interface.h types.h
