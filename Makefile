
CFLAGS=-g -W -Wall -Wno-unused -O2

par: checkpar.o makepar.o rwpar.o rs.o md5.o fileops.o main.o
	$(CC) -o $@ $^

clean:
	rm -f core par *.o

all: par

install: par
	install par ~/bin/

main.o: main.c checkpar.h types.h par.h rwpar.h
checkpar.o: checkpar.c checkpar.h util.h fileops.h types.h endian.h par.h
makepar.o: makepar.c makepar.h util.h fileops.h types.h endian.h par.h
md5.o: md5.c md5.h types.h
fileops.o: fileops.c fileops.h types.h util.h par.h
rwpar.o: rwpar.c rwpar.h par.h rs.h
rs.o: rs.c rs.h types.h fileops.h
