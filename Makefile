#!/bin/gmake

CC=gcc
CFLAGS=-I/usr/pkg/include/
LDFLAGS=-L/usr/pkg/lib/ -lm -lmpich

FILES=bit book check cluster cmd cmdan cmddbg cmddef cmduci cmdxb debug epd \
	eval evaleg evalinit evalks evalpawns evalpieces gen globals hash init	\
	input make output perft pgn ponder probe print rand regress search		\
	search2 searchroot see select smp smp2 stats test time tune unmake		\
	verify zct

HFILES=zct.h functions.h globals.h bit.h book.h cluster.h cmd.h debug.h		\
	eval.h pgn.h probe.h smp.h stats.h

CFILES=${addsuffix .c, ${FILES}}

OFILES=${addsuffix .o, ${FILES}}

# Different build options: release, debug, fast debug
OBJDIR=OBJ

OBJDIRRELEASE=${OBJDIR}/RELEASE
CFLAGSRELEASE=${CFLAGS} -Wall -O3
OFILESRELEASE=${addprefix ${OBJDIRRELEASE}/, ${OFILES}}

OBJDIRDEBUG=${OBJDIR}/DEBUG
CFLAGSDEBUG=${CFLAGS} -Wall -g
OFILESDEBUG=${addprefix ${OBJDIRDEBUG}/, ${OFILES}}

OBJDIRFDEBUG=${OBJDIR}/FDEBUG
CFLAGSFDEBUG=${CFLAGSRELEASE} -g
OFILESFDEBUG=${addprefix ${OBJDIRFDEBUG}/, ${OFILES}}

# Build methods
all: ${OBJDIRRELEASE} ${OFILESRELEASE} .dependr
	${CC} ${CFLAGSRELEASE} ${OFILESRELEASE} ${LDFLAGS} -o zct

debug: ${OBJDIRDEBUG} ${OFILESDEBUG} .dependd
	${CC} ${CFLAGSDEBUG} ${OFILESDEBUG} ${LDFLAGS} -o zct

fdebug: ${OBJDIRFDEBUG} ${OFILESFDEBUG} .dependf
	${CC} ${CFLAGSFDEBUG} ${OFILESFDEBUG} ${LDFLAGS} -o zct

release: ${CFILES} ${HFILES}
	cat ${CFILES} > .temp.c
	${CC} ${CFLAGSRELEASE} .temp.c ${LDFLAGS} -o zct
	rm -f .temp.c

distribution: cleano clean
	zip zct2491.zip *.c *.h ZCT.ini nbook.zbk Makefile COPYING.txt README.txt \
		CHANGES.txt
	
-include .dependr
-include .dependd
-include .dependf

${OBJDIRRELEASE}/%.o: %.c
	${CC} ${CFLAGSRELEASE} -c $< -o $@

${OBJDIRDEBUG}/%.o: %.c
	${CC} ${CFLAGSDEBUG} -c $< -o $@

${OBJDIRFDEBUG}/%.o: %.c
	${CC} ${CFLAGSFDEBUG} -c $< -o $@

# Object directories
${OBJDIRRELEASE} ${OBJDIRFDEBUG} ${OBJDIRDEBUG}: ${OBJDIR}

${OBJDIR}:
	mkdir -p ${OBJDIR}

${OBJDIRRELEASE}:
	mkdir -p ${OBJDIRRELEASE}

${OBJDIRDEBUG}:
	mkdir -p ${OBJDIRDEBUG}

${OBJDIRFDEBUG}:
	mkdir -p ${OBJDIRFDEBUG}

# Clean methods
clean:
	-rm -f ${OBJDIRRELEASE}/* ${OBJDIRDEBUG}/* ${OBJDIRFDEBUG}/* zct logfile.txt

cleano:
	-rm -f ${OBJDIRRELEASE}/* ${OBJDIRDEBUG}/* ${OBJDIRFDEBUG}/* *~ logfile.txt

# Dependency file generation
.dependr: ${CFILES}
	@rm -f .dependr
	@for i in ${FILES}; do \
		${CC} -MM -MT "${OBJDIRRELEASE}/$$i.o" $$i.c >> .dependr; \
	done

.dependd: ${CFILES}
	@rm -f .dependd
	@for i in ${FILES}; do \
		${CC} -MM -MT "${OBJDIRDEBUG}/$$i.o" $$i.c >> .dependd; \
	done

.dependf: ${CFILES}
	@rm -f .dependf
	@for i in ${FILES}; do \
		${CC} -MM -MT "${OBJDIRFDEBUG}/$$i.o" $$i.c >> .dependf; \
	done
