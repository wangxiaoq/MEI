CFLAGS := -Os -g -Wall

OBJ := memerr-inject del-inject
SRC := memerr-inject.c del-inject.c
CLEAN := ${OBJ} memerr-inject del-inject .depend
DISTCLEAN := .depend .gdb_history

.PHONY: clean depend install

ALL: ${OBJ}

clean:
	rm -f ${CLEAN}
	rm *.o

distclean: clean
	rm -f ${DISTCLEAN} *~

depend: .depend

.depend: ${SRC} ${GENSRC}
	${CC} -MM -DDEPS_RUN -I. ${SRC} ${GENSRC} > .depend.X && \
		mv .depend.X .depend

Makefile: .depend

include .depend
