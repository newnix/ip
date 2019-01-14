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

## Simply run some static analysis on the codebase
check: ${SRC}
	clang-tidy-devel ${SRC}

debug: ${SRC}
	$(CC) ${DBG} ${CFLAGS} ${LDFLAGS} ${SRC} -o ${DESTDIR}${TARGET}
	$(DESTDIR)$(TARGET) ${HELP}

install: ${SRC}
	$(CC) ${CFLAGS} ${LDFLAGS} ${SRC} -o ${DESTDIR}${TARGET}
	@strip -s ${DESTDIR}${TARGET}
	$(DESTDIR)$(TARGET) ${HELP}

## Simple means of displaying to the user what's being done here
help: 
	@printf "CFLAGS:\t\t%s\nLDFLAGS:\t%s\nCC:\t\t%s\nLD:\t\t%s\nSOURCE:\t\t%s\nINSTALL:\t%s%s\n\nRun \`%s Makefile\` to change these settings." "${CFLAGS}" "${LDFLAGS}" "${CC}" "${LD}" "${SRC}" "${DESTDIR}" "${TARGET}" "${EDITOR}"
