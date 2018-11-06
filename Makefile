.POSIX:

PREFIX = ${HOME}
DESTDIR = ${PREFIX}/bin/
TARGET = ip 
SRC = ip.c

CFLAGS = -Wall -Wextra -pedantic -std=c99 -fpic -fpie -fPIC -fPIE -Os
LDFLAGS = -Wl,--gc-sections,-icf=all -z relro -z combreloc -z now
DBG = -ggdb
CC = clang-devel
LD = ld.lld-devel
HELP = -h

debug: ${SRC}
	$(CC) ${DBG} ${CFLAGS} ${LDFLAGS} ${SRC} -o ${DESTDIR}${TARGET}
	$(DESTDIR)$(TARGET) ${HELP}

install: ${SRC}
	$(CC) ${CFLAGS} ${LDFLAGS} ${SRC} -o ${DESTDIR}${TARGET}
	@strip -s ${DESTDIR}${TARGET}
	$(DESTDIR)$(TARGET) ${HELP}
